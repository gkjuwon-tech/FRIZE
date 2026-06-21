/* FRIZE Landing — Interactions */
(function () {
  'use strict';

  var gnb = document.getElementById('gnb');
  var burger = document.getElementById('gnbBurger');
  var nav = document.getElementById('gnbNav');

  /* ── GNB scroll ── */
  window.addEventListener('scroll', function () {
    gnb.classList.toggle('gnb--solid', window.scrollY > 48);
  }, { passive: true });

  /* ── Mobile menu ── */
  if (burger && nav) {
    burger.addEventListener('click', function () {
      burger.classList.toggle('is-open');
      nav.classList.toggle('is-open');
    });
    nav.querySelectorAll('a').forEach(function (a) {
      a.addEventListener('click', function () {
        burger.classList.remove('is-open');
        nav.classList.remove('is-open');
      });
    });
  }

  /* ── Scroll reveal ── */
  var els = document.querySelectorAll('[data-reveal]');
  var io = new IntersectionObserver(function (entries) {
    entries.forEach(function (e) {
      if (e.isIntersecting) {
        e.target.classList.add('is-v');
        io.unobserve(e.target);
      }
    });
  }, { threshold: 0.12 });

  els.forEach(function (el) { io.observe(el); });

  /* ── Smooth anchor ── */
  document.querySelectorAll('a[href^="#"]').forEach(function (a) {
    a.addEventListener('click', function (e) {
      var t = document.querySelector(this.getAttribute('href'));
      if (t) {
        e.preventDefault();
        window.scrollTo({ top: t.offsetTop - 64, behavior: 'smooth' });
      }
    });
  });

  /* ── Form stub ── */
  var f = document.getElementById('demoForm');
  if (f) f.addEventListener('submit', function (e) {
    e.preventDefault();
    var b = f.querySelector('button');
    b.textContent = '전송 완료';
    b.disabled = true;
    setTimeout(function () { b.textContent = '요청 보내기'; b.disabled = false; f.reset(); }, 2500);
  });
})();
