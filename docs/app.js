(function(){
  const path = (location.pathname.split('/').pop() || 'index.html');
  document.querySelectorAll('[data-nav]').forEach(a=>{
    if(a.getAttribute('href') === path) a.classList.add('active');
  });
})();
