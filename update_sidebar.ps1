$files = Get-ChildItem "site\*.html"
foreach ($file in $files) {
    if ($file.Name -ne "docs_sqlite.html") {
        $content = Get-Content $file.FullName -Raw
        $content = $content -replace '(<li><a href="docs_ui_engine.html"[^>]*>11\. UI Engine</a></li>\s*)</ul>', '$1    <li><a href="docs_sqlite.html">12. SQLite Database</a></li>`n                </ul>'
        Set-Content $file.FullName $content
    }
}
