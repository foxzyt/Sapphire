document.addEventListener('DOMContentLoaded', () => {
    // Setup interactive snippet buttons
    const copyButtons = document.querySelectorAll('.btn-copy');
    const runButtons = document.querySelectorAll('.btn-run');

    copyButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const container = btn.closest('.snippet-container');
            const code = container.querySelector('code').innerText;
            
            navigator.clipboard.writeText(code).then(() => {
                const originalText = btn.innerText;
                btn.innerText = 'Copied!';
                setTimeout(() => btn.innerText = originalText, 2000);
            });
        });
    });

    runButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const container = btn.closest('.snippet-container');
            const output = container.querySelector('.output-container');
            
            if (output) {
                if (output.classList.contains('show')) {
                    output.classList.remove('show');
                    btn.innerText = 'Run / Output';
                } else {
                    output.classList.add('show');
                    btn.innerText = 'Hide Output';
                }
            }
        });
    });
});
