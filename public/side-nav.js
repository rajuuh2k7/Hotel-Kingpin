/* HOTEL KINGPIN: accessible slide-out menu + optional touch-edge swipe. */
(() => {
  const toggle = document.getElementById('menu-toggle');
  const drawer = document.getElementById('kingpin-drawer');
  const overlay = document.getElementById('drawer-overlay');
  const closeButton = document.getElementById('drawer-close');
  if (!toggle || !drawer || !overlay || !closeButton) return;
  let opened = false;
  let returnFocus = null;
  const setOpen = (value, focus = true) => {
    if (opened === value) return;
    opened = value;
    if (opened) {
      returnFocus = document.activeElement;
      drawer.inert = false;
      drawer.setAttribute('aria-hidden', 'false');
      overlay.hidden = false;
      requestAnimationFrame(() => overlay.classList.add('is-visible'));
      drawer.classList.add('is-open');
      document.body.classList.add('drawer-open');
      toggle.setAttribute('aria-expanded', 'true');
      toggle.setAttribute('aria-label', 'Close navigation menu');
      if (focus) closeButton.focus();
    } else {
      drawer.classList.remove('is-open');
      overlay.classList.remove('is-visible');
      drawer.setAttribute('aria-hidden', 'true');
      drawer.inert = true;
      document.body.classList.remove('drawer-open');
      toggle.setAttribute('aria-expanded', 'false');
      toggle.setAttribute('aria-label', 'Open navigation menu');
      overlay.hidden = true;
      if (focus && returnFocus && document.contains(returnFocus)) returnFocus.focus();
    }
  };
  toggle.addEventListener('click', () => setOpen(!opened));
  closeButton.addEventListener('click', () => setOpen(false));
  overlay.addEventListener('click', () => setOpen(false));
  drawer.querySelectorAll('a').forEach(link => link.addEventListener('click', () => setOpen(false, false)));
  document.addEventListener('keydown', event => {
    if (!opened) return;
    if (event.key === 'Escape') setOpen(false);
    if (event.key === 'Tab') {
      const focused = [...drawer.querySelectorAll('a, button')].filter(el => !el.disabled);
      const first = focused[0], last = focused[focused.length - 1];
      if (event.shiftKey && document.activeElement === first) {event.preventDefault(); last.focus();}
      else if (!event.shiftKey && document.activeElement === last) {event.preventDefault(); first.focus();}
    }
  });
  let touchStart = null;
  document.addEventListener('touchstart', event => {
    if (event.touches.length !== 1) return;
    const t = event.touches[0];
    if (opened || t.clientX < 30) touchStart = {x: t.clientX, y: t.clientY};
  }, {passive: true});
  document.addEventListener('touchend', event => {
    if (!touchStart || event.changedTouches.length !== 1) return;
    const t = event.changedTouches[0], dx = t.clientX - touchStart.x;
    const dy = Math.abs(t.clientY - touchStart.y);
    if (dy < 55 && Math.abs(dx) > 70) {
      if (!opened && dx > 0) setOpen(true, false);
      else if (opened && dx < 0) setOpen(false, false);
    }
    touchStart = null;
  }, {passive: true});
  const route = window.location.pathname;
  drawer.querySelectorAll('a').forEach(link => {
    const dest = new URL(link.href).pathname;
    if ((dest !== '/' && dest === route) || (dest === '/' && route === '/')) link.setAttribute('aria-current', 'page');
  });
})();
