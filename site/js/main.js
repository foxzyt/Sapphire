/* ================================================================
   Sapphire Docs — Interactive JS
   ================================================================ */
document.addEventListener('DOMContentLoaded', () => {

    // ─── Theme Toggle ──────────────────────────────────────────────
    const DARK_KEY = 'sapphire-theme';

    function applyTheme(isDark) {
        document.body.classList.toggle('dark-mode', isDark);
        document.querySelectorAll('#theme-toggle, #sidebar-theme-toggle').forEach(btn => {
            btn.textContent = isDark ? '🌙' : '☀️';
        });
        localStorage.setItem(DARK_KEY, isDark ? 'dark' : 'light');
    }

    // Load saved preference (default: dark)
    const saved = localStorage.getItem(DARK_KEY);
    applyTheme(saved !== 'light');

    function toggleTheme() {
        applyTheme(!document.body.classList.contains('dark-mode'));
    }

    document.querySelectorAll('#theme-toggle, #sidebar-theme-toggle').forEach(btn => {
        btn?.addEventListener('click', toggleTheme);
    });

    // ─── Mobile Sidebar ─────────────────────────────────────────────
    const sidebar  = document.querySelector('.sidebar');
    const overlay  = document.querySelector('.sidebar-overlay');
    const menuBtn  = document.querySelector('.sidebar-toggle');

    function openSidebar()  {
        sidebar?.classList.add('open');
        overlay?.classList.add('show');
    }
    function closeSidebar() {
        sidebar?.classList.remove('open');
        overlay?.classList.remove('show');
    }

    menuBtn?.addEventListener('click', openSidebar);
    overlay?.addEventListener('click', closeSidebar);

    // Close on nav link click (mobile)
    sidebar?.querySelectorAll('a').forEach(a => {
        a.addEventListener('click', () => {
            if (window.innerWidth <= 900) closeSidebar();
        });
    });

    // ─── Copy Buttons ───────────────────────────────────────────────
    document.querySelectorAll('.btn-copy').forEach(btn => {
        btn.addEventListener('click', () => {
            const container = btn.closest('.snippet-container');
            const code = container?.querySelector('code');
            if (!code) return;

            navigator.clipboard.writeText(code.innerText).then(() => {
                const orig = btn.textContent;
                btn.textContent = '✓ Copied!';
                btn.classList.add('copied');
                setTimeout(() => {
                    btn.textContent = orig;
                    btn.classList.remove('copied');
                }, 2000);
            }).catch(() => {
                btn.textContent = 'Error';
                setTimeout(() => btn.textContent = orig, 2000);
            });
        });
    });

    // ─── Run / Output Toggle ─────────────────────────────────────────
    document.querySelectorAll('.btn-run').forEach(btn => {
        btn.addEventListener('click', () => {
            const container = btn.closest('.snippet-container');
            const output    = container?.querySelector('.output-container');
            if (!output) return;

            const isOpen = output.classList.contains('show');
            output.classList.toggle('show', !isOpen);
            btn.textContent = isOpen ? 'Run ▶' : 'Hide';
        });
    });

    // ─── Article ToC — Scroll Spy ────────────────────────────────────
    const tocLinks = document.querySelectorAll('.article-toc a');

    if (tocLinks.length > 0) {
        const targets = [];
        tocLinks.forEach(link => {
            const href = link.getAttribute('href');
            if (href?.startsWith('#')) {
                const el = document.getElementById(href.slice(1));
                if (el) targets.push({ link, el });
            }
        });

        const updateToc = () => {
            let current = null;
            targets.forEach(({ el, link }) => {
                const rect = el.getBoundingClientRect();
                if (rect.top <= 120) current = link;
                link.classList.remove('toc-active');
            });
            if (current) current.classList.add('toc-active');
        };

        window.addEventListener('scroll', updateToc, { passive: true });
        updateToc(); // initial call
    }

    // ─── Scroll Reveal ───────────────────────────────────────────────
    const revealIo = new IntersectionObserver((entries) => {
        entries.forEach(e => {
            if (e.isIntersecting) {
                e.target.classList.add('visible');
                revealIo.unobserve(e.target);
            }
        });
    }, { threshold: 0.07 });

    document.querySelectorAll('.reveal').forEach(el => revealIo.observe(el));

    // ─── Sidebar Active Link ──────────────────────────────────────────
    const currentPath = location.pathname.split('/').pop() || 'index.html';
    document.querySelectorAll('.sidebar nav a').forEach(a => {
        const href = a.getAttribute('href')?.split('/').pop();
        if (href && href === currentPath) {
            a.classList.add('active');
        }
    });

    // ─── Smooth scroll for all #anchor links ─────────────────────────
    document.querySelectorAll('a[href^="#"]').forEach(a => {
        a.addEventListener('click', e => {
            const target = document.querySelector(a.getAttribute('href'));
            if (!target) return;
            e.preventDefault();
            target.scrollIntoView({ behavior: 'smooth', block: 'start' });
        });
    });

    // ─── Inline code copy on double-click ────────────────────────────
    document.querySelectorAll('p code, li code, td code').forEach(el => {
        el.style.cursor = 'pointer';
        el.title = 'Double-click to copy';
        el.addEventListener('dblclick', () => {
            navigator.clipboard.writeText(el.textContent).then(() => {
                const orig = el.style.background;
                el.style.background = 'rgba(34,197,94,0.2)';
                setTimeout(() => el.style.background = orig, 800);
            });
        });
    });

});
