// Builds the shared shell (top bar, sidebar, right-hand TOC, pager) around each page's <main>.
(function () {
  const NAV = [
    { title: 'Getting started', items: [
      ['index.html', 'Overview'],
      ['installation.html', 'Installation'],
      ['quickstart.html', 'Quick start'],
    ] },
    { title: 'User guide', items: [
      ['navigation.html', 'Navigating the viewer'],
      ['layout-dsl.html', 'Layout DSL (YAML)'],
      ['formats.html', 'Input formats'],
      ['launcher.html', 'Launcher, dev mode, export'],
    ] },
    { title: 'Examples', items: [
      ['examples.html', 'Demo and ProcBase'],
      ['demo/index.html?sample=layout', 'Open the live demo ↗'],
    ] },
    { title: 'Developer guide', items: [
      ['architecture.html', 'Architecture and invariants'],
      ['contributing.html', 'Contributing'],
    ] },
  ];
  const REPO = 'https://github.com/AnthonyBSong/VeriLens';
  const here = location.pathname.split('/').pop() || 'index.html';

  const top = document.createElement('header');
  top.className = 'topbar';
  top.innerHTML = `<a class="brand" href="index.html">Veri<span>Lens</span></a><span class="tagline">interactive RTL schematics for Verilog / SystemVerilog</span><span class="grow"></span>` +
    `<a class="btn" href="${REPO}" target="_blank" rel="noopener">GitHub</a><a class="btn primary" href="demo/index.html?sample=layout" target="_blank" rel="noopener">Live demo</a>`;
  document.body.prepend(top);

  const side = document.createElement('nav');
  side.className = 'sidebar';
  side.innerHTML = NAV.map((s) => `<h4>${s.title}</h4>` + s.items.map(([href, label]) =>
    `<a href="${href}" class="${href === here ? 'active' : ''}">${label}</a>`).join('')).join('');
  document.body.insertBefore(side, document.querySelector('main'));

  const main = document.querySelector('main');
  main.classList.add('content');
  const heads = [...main.querySelectorAll('h2, h3')];
  if (heads.length > 1) {
    const toc = document.createElement('aside');
    toc.className = 'toc';
    toc.innerHTML = '<h5>On this page</h5>' + heads.map((h) => {
      if (!h.id) h.id = h.textContent.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/(^-|-$)/g, '');
      return `<a class="${h.tagName.toLowerCase()}" href="#${h.id}">${h.textContent}</a>`;
    }).join('');
    document.body.appendChild(toc);
  }

  const flat = NAV.flatMap((s) => s.items).filter(([h]) => !h.includes('demo/'));
  const i = flat.findIndex(([h]) => h === here);
  if (i >= 0) {
    const pager = document.createElement('div');
    pager.className = 'pager';
    pager.innerHTML = `<span>${i > 0 ? `← <a href="${flat[i - 1][0]}">${flat[i - 1][1]}</a>` : ''}</span><span>${i < flat.length - 1 ? `<a href="${flat[i + 1][0]}">${flat[i + 1][1]}</a> →` : ''}</span>`;
    main.appendChild(pager);
  }
  const foot = document.createElement('footer');
  foot.className = 'site';
  foot.textContent = 'VeriLens documentation · MIT License · built as plain HTML on the gh-pages branch';
  main.appendChild(foot);
})();
