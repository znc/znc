function floodprotection_change() {
	var protection = document.getElementById('floodprotection_checkbox');
	var rate = document.getElementById('floodrate');
	var burst = document.getElementById('floodburst');
	if (protection.checked) {
		rate.removeAttribute('disabled');
		burst.removeAttribute('disabled');
	} else {
		rate.disabled = 'disabled';
		burst.disabled = 'disabled';
	}
}

