// Wraps each page's <main> with the logo header, the page links and the footer.
(function () {
  const PAGES = [
    ['index.html', 'Overview'],
    ['installation.html', 'Installation'],
    ['quickstart.html', 'Quick start'],
    ['layout-dsl.html', 'Layout DSL'],
    ['examples.html', 'Examples'],
    ['contributing.html', 'Contributing'],
  ];
  const REPO = 'https://github.com/AnthonyBSong/VeriLens';
  const here = location.pathname.split('/').pop() || 'index.html';
  const main = document.querySelector('main');
  main.classList.add('container');

  const header = document.createElement('div');
  header.className = 'site-header';
  header.innerHTML = `<a href="index.html"><img src="assets/img/logo.png" alt="VeriLens"></a>` +
    `<nav class="site-nav">` + PAGES.map(([href, label]) =>
      `<a href="${href}" class="${href === here ? 'active' : ''}">${label}</a>`).join('') +
    `<a href="${REPO}" target="_blank" rel="noopener">GitHub ↗</a></nav>`;
  main.prepend(header);

  // stable anchors for in-page links
  main.querySelectorAll('h2, h3').forEach((h) => {
    if (!h.id) h.id = h.textContent.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/(^-|-$)/g, '');
  });

  const foot = document.createElement('footer');
  foot.className = 'site';
  foot.textContent = 'VeriLens documentation · MIT License · plain HTML on the gh-pages branch';
  main.appendChild(foot);
})();
