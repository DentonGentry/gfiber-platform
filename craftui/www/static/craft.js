CraftUI = function() {
  // Do some feature sniffing for dependencies and return if not supported.
  if (!window.XMLHttpRequest ||
      !document.querySelector ||
      !Element.prototype.addEventListener ||
      !('classList' in document.createElement('_'))) {
    document.documentElement.classList.add('unsupported');
    return;
  }

  // Initialize the info.
  CraftUI.getInfo();

  // Refresh data periodically.
  window.setInterval(CraftUI.getInfo, 5000);
};

CraftUI.info = {checksum: 0};

CraftUI.updateField = function(key, val) {
  var el = document.getElementById(key);
  if (el == null) {
    self.unhandled += key + '=' + val + '; ';
    return;
  }
  el.innerHTML = ''; // Clear the field.
  // For objects, create an unordered list and append the values as list items.
  if (val && typeof val === 'object') {
    var ul = document.createElement('ul');
    for (key in val) {
      var li = document.createElement('li');
      var primary = document.createTextNode(key + ' ');
      li.appendChild(primary);
      var secondary = document.createElement('span');
      secondary.textContent = val[key];
      li.appendChild(secondary);
      ul.appendChild(li);
    }
    // If the unordered list has children, append it and return.
    if (ul.hasChildNodes()) {
      el.appendChild(ul);
      return;
    } else {
      val = 'N/A';
    }
  }
  el.appendChild(document.createTextNode(val));
};

CraftUI.flattenAndUpdateFields = function(jsonmap, prefix) {
  for (var key in jsonmap) {
    var val = jsonmap[key];
    if (typeof val !== 'object') {
      CraftUI.updateField(prefix + key, jsonmap[key]);
    } else {
      CraftUI.flattenAndUpdateFields(val, prefix + key + '/')
    }
  }
};

CraftUI.getInfo = function() {
  // Request info, set the connected status, and update the fields.
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    self.unhandled = '';
    if (xhr.readyState == 4 && xhr.status == 200) {
      var list = JSON.parse(xhr.responseText);
      CraftUI.flattenAndUpdateFields(list, '');
    }
    CraftUI.updateField('unhandled', self.unhandled);
  };
  var payload = [];
  payload.push('checksum=' + encodeURIComponent(CraftUI.info.checksum));
  payload.push('_=' + encodeURIComponent((new Date()).getTime()));
  xhr.open('get', 'content.json?' + payload.join('&'), true);
  xhr.send();
};

CraftUI.config = function(key, activate) {
  // POST as json
  var el = document.getElementById(key);
  var value = el.value;
  var xhr = new XMLHttpRequest();
  xhr.open('post', 'content.json');
  xhr.setRequestHeader('Content-Type', 'application/json; charset=UTF-8');
  var data;
  if (activate) {
    data = { config: [ { [key + "_activate"]: "true" } ] };
  } else {
    data = { config: [ { [key]: value } ] };
  }
  var txt = JSON.stringify(data);
  var resultid = key + "_result"
  var el = document.getElementById(resultid);
  xhr.onload = function(e) {
    var json = JSON.parse(xhr.responseText);
    if (json.error == 0) {
      el.innerHTML = "Success!";
    } else {
      el.innerHTML = "Error: " + json.errorstring;
    }
    CraftUI.getInfo();
  }
  xhr.onerror = function(e) {
    el.innerHTML = xhr.statusText + xhr.responseText;
  }
  xhr.send(txt);
};

new CraftUI();
