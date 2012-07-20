function updateUser() {
	var select = document.getElementById('selectnetwork');
	var opt = select.options[select.selectedIndex];
	document.getElementById('user').value = opt.parentNode.getAttribute('label');
}

function init() {
	updateUser();
	document.getElementById('networklabel').firstChild.nodeValue = 'Network:';
	document.getElementById('userblock').removeAttribute('style');
}
