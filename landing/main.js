/* ═══════════════════════════════════════
   FRIZE Landing — Interactions
   ═══════════════════════════════════════ */

(function () {
  'use strict';

  /* ── GNB: Scroll Detection ── */
  var gnb = document.getElementById('gnb');
  var lastScroll = 0;

  function onScroll() {
    var y = window.scrollY;
    if (y > 60) {
      gnb.classList.add('gnb--scrolled');
    } else {
      gnb.classList.remove('gnb--scrolled');
    }
    lastScroll = y;
  }

  window.addEventListener('scroll', onScroll, { passive: true });
  onScroll();

  /* ── GNB: Mobile Menu ── */
  var burger = document.getElementById('gnbBurger');
  var nav = document.getElementById('gnbNav');

  if (burger && nav) {
    burger.addEventListener('click', function () {
      burger.classList.toggle('is-open');
      nav.classList.toggle('is-open');
    });

    nav.querySelectorAll('.gnb__link').forEach(function (link) {
      link.addEventListener('click', function () {
        burger.classList.remove('is-open');
        nav.classList.remove('is-open');
      });
    });
  }

  /* ── Scroll Reveal ── */
  var reveals = document.querySelectorAll('[data-reveal]');

  function checkReveal() {
    var windowH = window.innerHeight;
    reveals.forEach(function (el) {
      var rect = el.getBoundingClientRect();
      if (rect.top < windowH * 0.88) {
        el.classList.add('is-visible');
      }
    });
  }

  window.addEventListener('scroll', checkReveal, { passive: true });
  window.addEventListener('resize', checkReveal, { passive: true });
  checkReveal();

  /* ── Smooth Anchor Scroll ── */
  document.querySelectorAll('a[href^="#"]').forEach(function (anchor) {
    anchor.addEventListener('click', function (e) {
      var target = document.querySelector(this.getAttribute('href'));
      if (target) {
        e.preventDefault();
        var offset = gnb ? gnb.offsetHeight : 72;
        var top = target.getBoundingClientRect().top + window.scrollY - offset;
        window.scrollTo({ top: top, behavior: 'smooth' });
      }
    });
  });

  /* ── Demo Form (Stub) ── */
  var form = document.getElementById('demoForm');
  if (form) {
    form.addEventListener('submit', function (e) {
      e.preventDefault();
      var btn = form.querySelector('button[type="submit"]');
      var origText = btn.innerHTML;
      btn.innerHTML = '전송 완료';
      btn.disabled = true;
      btn.style.opacity = '.6';
      setTimeout(function () {
        btn.innerHTML = origText;
        btn.disabled = false;
        btn.style.opacity = '1';
        form.reset();
      }, 3000);
    });
  }
})();
