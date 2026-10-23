const copyButton = document.querySelector('.copy');
const copyStatus = document.querySelector('.copy-status');
copyButton.hidden = false;
copyButton.addEventListener('click', async () => {
  try {
    await navigator.clipboard.writeText(document.querySelector('#install-command').textContent);
    copyStatus.textContent = 'Copied. Paste into your terminal when you are ready to install.';
  } catch {
    copyStatus.textContent = 'Clipboard access is unavailable. Select the command above and copy it manually.';
  }
});
