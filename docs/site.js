const ru = document.documentElement.lang === 'ru';
const T = ru
  ? { copy: 'Копировать', copied: 'Скопировано', manual: 'Выделите и скопируйте',
      win: 'Скачать для Windows', rpm: 'Скачать для Fedora', linux: 'Скачать для Linux', other: 'Скачать' }
  : { copy: 'Copy', copied: 'Copied', manual: 'Select and copy',
      win: 'Download for Windows', rpm: 'Download for Fedora', linux: 'Download for Linux', other: 'Download' };

// Remember the language picked with the EN / RU switch, so the Russian redirect does not fight it.
document.querySelectorAll('[data-lang]').forEach(a => a.addEventListener('click', () => {
  try { localStorage.setItem('lang', a.dataset.lang); } catch {}
}));

// Copy buttons: commands in <pre> blocks and the wallet address.
document.querySelectorAll('pre').forEach(pre => {
  const box = document.createElement('div');
  box.className = 'code';
  pre.replaceWith(box);
  const b = document.createElement('button');
  b.className = 'copy'; b.type = 'button'; b.textContent = T.copy;
  b.dataset.copyText = pre.innerText;
  box.append(pre, b);
});
document.addEventListener('click', async e => {
  const b = e.target.closest('[data-copy], [data-copy-text]');
  if (!b) return;
  const text = b.dataset.copyText ?? document.querySelector(b.dataset.copy).textContent;
  try { await navigator.clipboard.writeText(text.trim()); b.textContent = T.copied; }
  catch { b.textContent = T.manual; }
  setTimeout(() => (b.textContent = T.copy), 1500);
});

// Point the download buttons at the files of the latest release; without JS or the API they open the releases page.
(async () => {
  const ua = navigator.userAgent;
  const os = /Windows/i.test(ua) ? 'win' : /Fedora|Red Hat/i.test(ua) ? 'rpm' : /Linux|X11/i.test(ua) ? 'linux' : 'other';
  const label = document.getElementById('download-label');
  label.textContent = T[os];
  try {
    const r = await fetch('https://api.github.com/repos/NikitaKnutov/superpupervpn/releases/latest');
    if (!r.ok) return;
    const rel = await r.json();
    const find = ext => rel.assets.find(a => a.name.endsWith(ext));
    document.querySelectorAll('[data-version]').forEach(el => (el.textContent = rel.tag_name));
    document.querySelectorAll('[data-asset]').forEach(a => {
      const asset = find(a.dataset.asset);
      if (asset) a.href = asset.browser_download_url;
    });
    // Generic Linux gets the releases page: .deb or .rpm depends on the distro.
    const main = { win: find('.msi'), rpm: find('.rpm') }[os];
    if (main) document.getElementById('download').href = main.browser_download_url;
  } catch { /* offline or rate-limited: keep the releases page links */ }
})();
