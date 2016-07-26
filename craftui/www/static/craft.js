CraftUI = function() {
  // Do some feature sniffing for dependencies and return if not supported.
  if (!window.XMLHttpRequest ||
      !document.querySelector ||
      !Element.prototype.addEventListener ||
      !('classList' in document.createElement('_'))) {
    document.documentElement.classList.add('unsupported');
    return;
  }
};

CraftUI.prototype.init = function() {
  this.info = {checksum: 0};
  this.am_sending = false

  // store history on some values
  this.history = {
    'radio/rx/rsl': { name: 'rsl-graph', count: 24, values: [] }
  };

  // Initialize the info.
  this.getInfo();

  // Refresh data periodically.
  var f = function() {
    this.ui.getInfo();
  };
  window.ui = this;
  window.setInterval(f, 5000);
};

CraftUI.prototype.updateGraphs = function() {
  for (var name in this.history) {
    var h = this.history[name];
    if (h.values.length == 0) {
      continue;
    }
    if (!h.graph) {
      h.graph = new Dygraph(document.getElementById(h.name), h.values, {
        xlabel: 'Time',
        ylabel: 'RSL',
        labels: [ 'Date', 'RSL' ],
      });
    }
    h.graph.updateOptions({ 'file': h.values });
  }
};

CraftUI.prototype.updateField = function(key, val) {
  // store history if requested
  var h = this.history[key];
  if (h) {
    h.values.push([ui.date, val]);
    if (h.values.length > h.count) {
      h.values = h.values.slice(-h.count);
    }
  }
  // find element, show on debug page if not used
  var el = document.getElementById(key);
  if (el == null) {
    this.unhandled += key + '=' + val + '; ';
    return;
  }
  // For IMG objects, set image
  if (el.src) {
    el.src = '/static/' + val;
    return;
  }
  // For objects, create an unordered list and append the values as list items.
  el.innerHTML = ''; // Clear the field.
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

CraftUI.prototype.flattenAndUpdateFields = function(jsonmap, prefix) {
  for (var key in jsonmap) {
    var val = jsonmap[key];
    if (typeof val !== 'object') {
      this.updateField(prefix + key, jsonmap[key]);
    } else {
      this.flattenAndUpdateFields(val, prefix + key + '/')
    }
  }
};

CraftUI.prototype.getInfo = function() {
  // Request info, set the connected status, and update the fields.
  if (this.am_sending) {
    return;
  }
  var peer_arg_on_peer = document.getElementById("peer_arg_on_peer").value;
  var xhr = new XMLHttpRequest();
  xhr.timeout = 2000;
  xhr.ui = this;
  xhr.onreadystatechange = function() {
    var ui = this.ui;
    if (xhr.readyState != 4) {
      return;
    }
    ui.unhandled = '';
    var led = 'red.gif';
    if (xhr.status == 200) {
      ui.date = new Date();
      var list = JSON.parse(xhr.responseText);
      ui.flattenAndUpdateFields(list, '');
      led = 'green.gif';
    } else {
      var leds = ['ACS', 'Switch', 'Modem', 'Radio', 'RSSI', 'MSE', 'Peer'];
      for (var i in leds) {
        ui.updateField('leds/' + leds[i], 'grey.gif')
      }
    }
    ui.updateField('unhandled', ui.unhandled);
    ui.updateField('leds/Craft', led)
    ui.updateGraphs();
    ui.am_sending = false
  };
  xhr.open('get', '/content.json' + peer_arg_on_peer, true);
  this.am_sending = true
  xhr.send();
};

CraftUI.prototype.config = function(key, activate, is_password) {
  // POST as json
  var peer_arg_on_peer = document.getElementById("peer_arg_on_peer").value;
  var el = document.getElementById(key);
  var action = "Configured";
  var xhr = new XMLHttpRequest();
  xhr.ui = this;
  xhr.open('post', '/content.json' + peer_arg_on_peer);
  xhr.setRequestHeader('Content-Type', 'application/json; charset=UTF-8');
  var data;
  if (is_password) {
    var value_admin = btoa(document.getElementById(key + "_admin").value);
    var value_new = btoa(document.getElementById(key + "_new").value);
    var value_confirm = btoa(document.getElementById(key + "_confirm").value);
    data = { config: [ { [key]: {
          "admin": value_admin,
          "new": value_new,
          "confirm": value_confirm,
        } } ] };
  } else if (activate) {
    data = { config: [ { [key + "_activate"]: "true" } ] };
    action = "Applied";
  } else {
    data = { config: [ { [key]: el.value } ] };
  }
  var txt = JSON.stringify(data);
  var resultid = key + "_result"
  var el = document.getElementById(resultid);
  xhr.onload = function(e) {
    var ui = this.ui;
    var json = JSON.parse(xhr.responseText);
    if (json.error == 0) {
      el.innerHTML = action + " successfully.";
    } else {
      el.innerHTML = "Error: " + json.errorstring;
    }
    ui.getInfo();
  }
  xhr.onerror = function(e) {
    el.innerHTML = xhr.statusText + xhr.responseText;
  }
  el.innerHTML = "sending...";
  xhr.send(txt);
};

var craftUI = new CraftUI();
craftUI.init();
