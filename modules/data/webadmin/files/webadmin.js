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

function make_sortable_table(table) {
	if (table.rows.length >= 1) { // Ensure that the table at least contains a row for the headings
		var headings = table.rows[0].getElementsByTagName("td");
		for (var i = 0; i < headings.length; i++) {
			// This function acts to scope the i variable, so we can pass it off
			// as the column_index, otherwise column_index would just be the max
			// value of i, every single time.
			(function (i) {
				var heading = headings[i];
				if (!heading.classList.contains("ignore-sort")) {
					heading.addEventListener("click", function () { // Bind a click event to the heading
						sort_table(this, i, table, headings);
					});
				}
			})(i);
		}
	}
}

function sort_table(clicked_column, column_index, table, headings) {
	for (var i = 0; i < headings.length; i++) {
		if (headings[i] != clicked_column) {
			headings[i].classList.remove("sorted");
			headings[i].classList.remove("reverse-sorted");
		}
	}
	var reverse = false;
	clicked_column.classList.toggle("reverse");
	if (clicked_column.classList.contains("sorted")) {
		reverse = true;
		clicked_column.classList.remove("sorted");
		clicked_column.classList.add("reverse-sorted");
	} else {
		clicked_column.classList.remove("reverse-sorted");
		clicked_column.classList.add("sorted");
	}

	// This array will contain tuples in the form [(value, row)] where value
	// is extracted from the column to be sorted by
	var rows_and_sortable_value = [];
	for (var i = 1, row; row = table.rows[i]; i++) {
		for (var j = 0, col; col = row.cells[j]; j++) {
			// If we're at the column index we want to sort by
			if (j === column_index) {
				var cell = row.getElementsByTagName("td")[j];
				var value = cell.innerHTML;
				rows_and_sortable_value.push([value, row]);
			}
		}
	}

	rows_and_sortable_value.sort(function (a, b) {
		// If both values are integers, sort by that else as strings
		if (isInt(a[0]) && isInt(b[0])) {
			return a[0] - b[0];
		} else {
			return b[0].localeCompare(a[0]);
		}
	});
	if (reverse) {
		rows_and_sortable_value.reverse();
	}

	var parent = table.rows[1].parentNode;
	for (var i = 0; i < rows_and_sortable_value.length; i++) {
		// Remove the existing entry for the row from the table
		parent.removeChild(rows_and_sortable_value[i][1]);
		// Insert at the first position, before the first child
		parent.insertBefore(rows_and_sortable_value[i][1], parent.firstChild);
	}
}

function isInt(value) {
	return !isNaN(value) && (function (x) {
		return (x | 0) === x;
	})(parseFloat(value))
}

function make_sortable() {
	var tables = document.querySelectorAll("table.sortable");
	for (var i = 0; i < tables.length; i++) {
    	make_sortable_table(tables[i]);
	}
}