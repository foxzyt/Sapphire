"""
Tests for .github/workflows/release.yml

These tests validate:
  1. The structural shape of the workflow (triggers, permissions, jobs,
     job dependencies/conditions, and key step configuration) using a YAML
     parser, since GitHub Actions workflows are plain YAML documents.
  2. The behavior of the non-trivial bash logic that is embedded in the
     workflow's `run:` blocks (version tag creation and "previous tag"
     lookup), by extracting those scripts from the parsed YAML and
     executing them against a throwaway git repository.

Run with:
    python3 -m unittest tests/workflows/test_release_workflow.py -v
"""
import os
import re
import subprocess
import tempfile
import unittest

import yaml

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
WORKFLOW_PATH = os.path.join(REPO_ROOT, ".github", "workflows", "release.yml")


def load_workflow():
    with open(WORKFLOW_PATH, "r", encoding="utf-8") as fh:
        return yaml.safe_load(fh)


def get_triggers(doc):
    """PyYAML (YAML 1.1) parses the bare `on:` key as the boolean True."""
    if "on" in doc:
        return doc["on"]
    return doc[True]


def get_job(doc, job_name):
    jobs = doc["jobs"]
    assert job_name in jobs, f"job {job_name!r} not found in workflow (have {list(jobs)})"
    return jobs[job_name]


def get_step(job, step_name):
    for step in job["steps"]:
        if step.get("name") == step_name:
            return step
    raise AssertionError(f"step {step_name!r} not found (have {[s.get('name') for s in job['steps']]})")


def run_bash(script, env, cwd):
    """Run a bash script (already GitHub-Actions-templated) and return CompletedProcess."""
    return subprocess.run(
        ["bash", "-c", script],
        cwd=cwd,
        env=env,
        capture_output=True,
        text=True,
        timeout=30,
    )


class ReleaseWorkflowStructureTest(unittest.TestCase):
    """Static/structural assertions about the parsed workflow document."""

    @classmethod
    def setUpClass(cls):
        cls.doc = load_workflow()
        cls.jobs = cls.doc["jobs"]

    def test_workflow_is_valid_yaml_with_expected_top_level_keys(self):
        self.assertIsInstance(self.doc, dict)
        self.assertEqual(self.doc.get("name"), "Release")
        self.assertIn("jobs", self.doc)

    def test_triggers_on_push_to_main_branch_only(self):
        triggers = get_triggers(self.doc)
        self.assertIn("push", triggers)
        self.assertEqual(triggers["push"], {"branches": ["main"]})

    def test_tag_based_trigger_was_removed(self):
        triggers = get_triggers(self.doc)
        self.assertNotIn("tags", triggers.get("push", {}))

    def test_workflow_dispatch_trigger_still_present(self):
        triggers = get_triggers(self.doc)
        self.assertIn("workflow_dispatch", triggers)

    def test_permissions_unchanged(self):
        self.assertEqual(
            self.doc["permissions"],
            {"contents": "write", "pages": "write", "id-token": "write"},
        )

    def test_expected_job_names_present(self):
        self.assertEqual(
            set(self.jobs.keys()), {"build", "generate-release", "deploy-pages"}
        )

    def test_legacy_build_and_release_job_removed(self):
        self.assertNotIn("build-and-release", self.jobs)


class BuildJobTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.doc = load_workflow()
        cls.job = get_job(cls.doc, "build")

    def test_runs_only_on_main_branch(self):
        self.assertEqual(self.job.get("if"), "github.ref == 'refs/heads/main'")

    def test_runs_on_windows(self):
        self.assertEqual(self.job.get("runs-on"), "windows-latest")

    def test_checkout_step_has_full_history(self):
        step = get_step(self.job, "Checkout")
        self.assertEqual(step["uses"], "actions/checkout@v4")
        self.assertEqual(step.get("with", {}).get("fetch-depth"), 0)

    def test_upload_build_artifacts_step_configuration(self):
        step = get_step(self.job, "Upload Build Artifacts")
        self.assertEqual(step["uses"], "actions/upload-artifact@v4")
        with_block = step["with"]
        self.assertEqual(with_block["name"], "sapphire-windows-build")
        self.assertEqual(with_block["path"], "sapphire-windows.zip")
        self.assertEqual(with_block["retention-days"], 1)

    def test_upload_artifacts_step_is_last_step(self):
        self.assertEqual(self.job["steps"][-1]["name"], "Upload Build Artifacts")

    def test_no_direct_publish_release_step_in_build_job(self):
        names = [s.get("name") for s in self.job["steps"]]
        self.assertNotIn("Publish GitHub Release", names)


class GenerateReleaseJobTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.doc = load_workflow()
        cls.job = get_job(cls.doc, "generate-release")

    def test_depends_on_build_job(self):
        self.assertEqual(self.job.get("needs"), "build")

    def test_runs_only_on_main_branch(self):
        self.assertEqual(self.job.get("if"), "github.ref == 'refs/heads/main'")

    def test_runs_on_ubuntu(self):
        self.assertEqual(self.job.get("runs-on"), "ubuntu-latest")

    def test_checkout_step_has_full_history(self):
        step = get_step(self.job, "Checkout")
        self.assertEqual(step["uses"], "actions/checkout@v4")
        self.assertEqual(step.get("with", {}).get("fetch-depth"), 0)

    def test_download_build_artifacts_step_matches_upload_name(self):
        build_job = get_job(self.doc, "build")
        upload_step = get_step(build_job, "Upload Build Artifacts")
        download_step = get_step(self.job, "Download Build Artifacts")
        self.assertEqual(download_step["uses"], "actions/download-artifact@v4")
        self.assertEqual(
            download_step["with"]["name"], upload_step["with"]["name"]
        )

    def test_create_version_tag_step_configuration(self):
        step = get_step(self.job, "Create Version Tag")
        self.assertEqual(step.get("id"), "tag")
        self.assertEqual(step.get("shell"), "bash")
        script = step["run"]
        self.assertIn('TAG="v1.0.${{ github.run_number }}"', script)
        self.assertIn("echo \"TAG=$TAG\" >> $GITHUB_ENV", script)
        self.assertIn("git tag \"$TAG\"", script)
        self.assertIn("git push origin \"$TAG\"", script)

    def test_fetch_previous_tag_history_excludes_new_tag(self):
        step = get_step(self.job, "Fetch previous tag history")
        script = step["run"]
        self.assertIn("git fetch --tags --force", script)
        # Regression check: the previous implementation used
        # `head -n 2 | tail -n 1` which broke once the current tag was
        # pushed before this step ran. The fix filters the newly created
        # tag out explicitly and takes the top of the sorted list.
        self.assertIn('grep -v "${{ env.TAG }}"', script)
        self.assertIn("head -n 1", script)
        self.assertNotIn("tail -n 1", script)

    def test_copilot_release_notes_step_is_non_blocking(self):
        step = get_step(self.job, "Run Copilot Release Notes")
        self.assertEqual(step.get("uses"), "github/copilot-release-notes@v1")
        self.assertTrue(step.get("continue-on-error"))

    def test_copilot_release_notes_uses_generated_tag_as_head_ref(self):
        step = get_step(self.job, "Run Copilot Release Notes")
        self.assertEqual(step["with"]["head-ref"], "${{ env.TAG }}")

    def test_prepare_release_body_has_github_token_env(self):
        step = get_step(self.job, "Prepare release body")
        self.assertEqual(
            step["env"].get("GITHUB_TOKEN"), "${{ secrets.GITHUB_TOKEN }}"
        )

    def test_prepare_release_body_falls_back_to_gh_api_generated_notes(self):
        step = get_step(self.job, "Prepare release body")
        script = step["run"]
        self.assertIn("gh api", script)
        self.assertIn("releases/generate-notes", script)

    def test_prepare_release_body_appends_examples_and_compile_instructions_unconditionally(self):
        step = get_step(self.job, "Prepare release body")
        script = step["run"]
        # These lines must run regardless of which branch (copilot vs.
        # fallback) produced the notes, and therefore must appear exactly
        # once, after the closing `fi` of the if/else block.
        self.assertEqual(script.count(".github/v1.0.8-examples.md"), 1)
        self.assertEqual(script.count("## How to Compile (MinGW)"), 1)
        # Match the closing `fi` keyword as a standalone line (not the
        # substring "fi" inside words like "config"/"configure").
        fi_match = list(re.finditer(r"^\s*fi\s*$", script, re.MULTILINE))
        self.assertTrue(fi_match, "no closing 'fi' found for the if/else block")
        fi_index = fi_match[-1].start()
        examples_index = script.index(".github/v1.0.8-examples.md")
        self.assertGreater(examples_index, fi_index)

    def test_publish_release_uses_generated_tag(self):
        step = get_step(self.job, "Publish GitHub Release")
        self.assertEqual(step["with"]["tag_name"], "${{ env.TAG }}")
        self.assertEqual(step["with"]["name"], "Sapphire ${{ env.TAG }}")


class DeployPagesJobTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.doc = load_workflow()
        cls.job = get_job(cls.doc, "deploy-pages")

    def test_depends_on_build_job(self):
        self.assertEqual(self.job.get("needs"), "build")

    def test_runs_only_on_main_branch(self):
        self.assertEqual(self.job.get("if"), "github.ref == 'refs/heads/main'")

    def test_declares_github_pages_environment(self):
        env = self.job.get("environment")
        self.assertIsInstance(env, dict)
        self.assertEqual(env.get("name"), "github-pages")
        self.assertEqual(env.get("url"), "${{ steps.deployment.outputs.page_url }}")

    def test_deploy_step_has_deployment_id(self):
        step = get_step(self.job, "Deploy to GitHub Pages")
        self.assertEqual(step.get("id"), "deployment")
        self.assertEqual(step.get("uses"), "actions/deploy-pages@v4")


class CreateVersionTagScriptBehaviorTest(unittest.TestCase):
    """Executes the extracted "Create Version Tag" bash logic against a real
    (throwaway) git repository to verify the tag it produces and pushes."""

    @classmethod
    def setUpClass(cls):
        doc = load_workflow()
        job = get_job(doc, "generate-release")
        cls.raw_script = get_step(job, "Create Version Tag")["run"]

    def _render(self, run_number):
        return self.raw_script.replace(
            "${{ github.run_number }}", str(run_number)
        )

    def _make_repo_with_remote(self, tmpdir):
        remote_dir = os.path.join(tmpdir, "remote.git")
        work_dir = os.path.join(tmpdir, "work")
        os.makedirs(remote_dir)
        os.makedirs(work_dir)
        subprocess.run(["git", "init", "--bare", remote_dir], check=True, capture_output=True)
        subprocess.run(["git", "init"], cwd=work_dir, check=True, capture_output=True)
        subprocess.run(
            ["git", "commit", "--allow-empty", "-m", "init"],
            cwd=work_dir,
            check=True,
            capture_output=True,
            env=self._git_author_env(),
        )
        subprocess.run(
            ["git", "remote", "add", "origin", remote_dir],
            cwd=work_dir,
            check=True,
            capture_output=True,
        )
        subprocess.run(
            ["git", "push", "-u", "origin", "HEAD:refs/heads/main"],
            cwd=work_dir,
            check=True,
            capture_output=True,
        )
        return work_dir

    @staticmethod
    def _git_author_env():
        env = os.environ.copy()
        env.update(
            GIT_AUTHOR_NAME="test",
            GIT_AUTHOR_EMAIL="test@example.com",
            GIT_COMMITTER_NAME="test",
            GIT_COMMITTER_EMAIL="test@example.com",
        )
        return env

    def test_generates_tag_with_expected_format_and_pushes_it(self):
        with tempfile.TemporaryDirectory() as tmp:
            work_dir = self._make_repo_with_remote(tmp)
            github_env_file = os.path.join(tmp, "github_env")
            open(github_env_file, "w").close()

            env = self._git_author_env()
            env["GITHUB_ENV"] = github_env_file

            result = run_bash(self._render(123), env=env, cwd=work_dir)

            self.assertEqual(result.returncode, 0, msg=result.stderr)
            with open(github_env_file) as fh:
                self.assertIn("TAG=v1.0.123", fh.read())

            tags = subprocess.run(
                ["git", "tag"], cwd=work_dir, capture_output=True, text=True, check=True
            ).stdout.split()
            self.assertIn("v1.0.123", tags)

            remote_dir = os.path.join(tmp, "remote.git")
            remote_tags = subprocess.run(
                ["git", "tag"], cwd=remote_dir, capture_output=True, text=True, check=True
            ).stdout.split()
            self.assertIn("v1.0.123", remote_tags, "tag was not pushed to origin")

    def test_different_run_numbers_produce_distinct_tags(self):
        with tempfile.TemporaryDirectory() as tmp:
            work_dir = self._make_repo_with_remote(tmp)
            github_env_file = os.path.join(tmp, "github_env")
            open(github_env_file, "w").close()
            env = self._git_author_env()
            env["GITHUB_ENV"] = github_env_file

            first = run_bash(self._render(1), env=env, cwd=work_dir)
            self.assertEqual(first.returncode, 0, msg=first.stderr)

            second = run_bash(self._render(2), env=env, cwd=work_dir)
            self.assertEqual(second.returncode, 0, msg=second.stderr)

            tags = set(
                subprocess.run(
                    ["git", "tag"], cwd=work_dir, capture_output=True, text=True, check=True
                ).stdout.split()
            )
            self.assertEqual({"v1.0.1", "v1.0.2"}, tags & {"v1.0.1", "v1.0.2"})


class FetchPreviousTagHistoryScriptBehaviorTest(unittest.TestCase):
    """Executes the extracted "Fetch previous tag history" bash logic to
    verify PREV_TAG resolution, including the fix for excluding the tag
    that was just created in the same run."""

    @classmethod
    def setUpClass(cls):
        doc = load_workflow()
        job = get_job(doc, "generate-release")
        cls.raw_script = get_step(job, "Fetch previous tag history")["run"]

    def _render(self, current_tag):
        return self.raw_script.replace("${{ env.TAG }}", current_tag)

    def _make_repo(self, tmpdir, tags):
        work_dir = os.path.join(tmpdir, "work")
        os.makedirs(work_dir)
        subprocess.run(["git", "init"], cwd=work_dir, check=True, capture_output=True)
        env = os.environ.copy()
        env.update(
            GIT_AUTHOR_NAME="test",
            GIT_AUTHOR_EMAIL="test@example.com",
            GIT_COMMITTER_NAME="test",
            GIT_COMMITTER_EMAIL="test@example.com",
        )
        for tag in tags:
            subprocess.run(
                ["git", "commit", "--allow-empty", "-m", tag],
                cwd=work_dir,
                check=True,
                capture_output=True,
                env=env,
            )
            subprocess.run(
                ["git", "tag", tag], cwd=work_dir, check=True, capture_output=True
            )
        return work_dir

    def _run_and_get_prev_tag(self, work_dir, current_tag):
        github_env_file = os.path.join(os.path.dirname(work_dir), "github_env")
        open(github_env_file, "w").close()
        env = os.environ.copy()
        env["GITHUB_ENV"] = github_env_file

        result = run_bash(self._render(current_tag), env=env, cwd=work_dir)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

        with open(github_env_file) as fh:
            content = fh.read()
        for line in content.splitlines():
            if line.startswith("PREV_TAG="):
                return line[len("PREV_TAG="):]
        return None

    def test_selects_highest_tag_excluding_the_current_one(self):
        with tempfile.TemporaryDirectory() as tmp:
            work_dir = self._make_repo(tmp, ["v1.0.1", "v1.0.2", "v1.0.3"])
            prev_tag = self._run_and_get_prev_tag(work_dir, "v1.0.3")
            self.assertEqual(prev_tag, "v1.0.2")

    def test_ignores_non_matching_current_tag_still_returns_highest(self):
        with tempfile.TemporaryDirectory() as tmp:
            work_dir = self._make_repo(tmp, ["v1.0.1", "v1.0.5", "v1.0.10"])
            # Simulate the newly created tag not yet existing locally in the
            # exact numeric form (defensive/edge case): highest remaining
            # tag should still be selected correctly (numeric sort, not
            # lexical: v1.0.10 > v1.0.5).
            prev_tag = self._run_and_get_prev_tag(work_dir, "v1.0.999")
            self.assertEqual(prev_tag, "v1.0.10")

    def test_returns_empty_when_no_previous_tag_exists(self):
        with tempfile.TemporaryDirectory() as tmp:
            work_dir = self._make_repo(tmp, ["v1.0.1"])
            prev_tag = self._run_and_get_prev_tag(work_dir, "v1.0.1")
            self.assertEqual(prev_tag, "")

    def test_returns_empty_when_no_tags_exist_at_all(self):
        with tempfile.TemporaryDirectory() as tmp:
            work_dir = self._make_repo(tmp, [])
            prev_tag = self._run_and_get_prev_tag(work_dir, "v1.0.1")
            self.assertEqual(prev_tag, "")


if __name__ == "__main__":
    unittest.main()