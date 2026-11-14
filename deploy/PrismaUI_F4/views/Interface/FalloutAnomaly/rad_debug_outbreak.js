var OUTBREAK_STAGES = ["Incubating", "Early", "Progress", "Final", "Recovered"];

function obEl(tag, cls, text) {
  var e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
}

function obRow(name, word, k, step, msg) {
  var row = obEl("div", "ob-row");
  row.appendChild(obEl("span", "ob-name", name));
  var stage = obEl("span", "ob-stage", word);
  if (k >= 0) stage.setAttribute("data-k", k);
  row.appendChild(stage);
  if (msg) {
    row.appendChild(obEl("span", "ob-msg", msg));
    return row;
  }
  var track = obEl("ol", "ob-track");
  for (var i = 0; i < OUTBREAK_STAGES.length; i++) {
    var li = obEl("li", i === step ? "cur" : (i < step ? "past" : ""));
    li.setAttribute("data-k", i);
    li.title = OUTBREAK_STAGES[i];
    track.appendChild(li);
  }
  row.appendChild(track);
  return row;
}

window.updateOutbreakStatus = function(json) {
  var data = json;
  if (typeof json === "string") { try { data = JSON.parse(json); } catch (e) { return; } }
  if (!data) return;
  var list = document.getElementById("outbreakCompanions");
  if (list) {
    var companions = Array.isArray(data.companions) ? data.companions : [];
    list.textContent = "";
    if (data.you) {
      var packed = Math.max(0, Math.min(23, Math.round(+data.you.d || 0)));
      var illId = Math.floor(packed / 10), illStage = packed % 10;
      var illName = SURV_DISEASES[illId] || "ILLNESS";
      if (illId === 0) list.appendChild(obRow("You", "Healthy", 0, -1));
      else list.appendChild(obRow("You", illName + " · " + OUTBREAK_STAGES[Math.min(illStage, 3)] + (illStage > 0 ? "" : " " + Math.round(+data.you.o || 0) + "%"), Math.min(illStage, 3), Math.min(illStage, 3)));
    }
    for (var i = 0; i < companions.length; i++) {
      var s = Math.round(+companions[i].s);
      var cname = String(companions[i].n || "Companion");
      if (!data.sick) list.appendChild(obRow(cname, "Unknown", -1, -1, "Unknown (RadOutbreak.esp missing)"));
      else if (s >= 0 && s < OUTBREAK_STAGES.length) list.appendChild(obRow(cname, OUTBREAK_STAGES[s], s, s));
      else list.appendChild(obRow(cname, "Healthy", 0, -1));
    }
    if (!companions.length) list.appendChild(obEl("div", "ob-empty", data.companionFaction ? "None loaded" : "Faction not found"));
  }
  var nearby = document.getElementById("outbreakNearby");
  if (nearby) {
    var counts = Array.isArray(data.nearby) ? data.nearby : [];
    var n = [], total = 0;
    for (var k = 0; k < OUTBREAK_STAGES.length; k++) {
      n[k] = Math.max(0, Math.round(+counts[k] || 0));
      total += n[k];
    }
    nearby.textContent = "";
    if (!total) {
      nearby.appendChild(obEl("div", "ob-empty", "No sick NPCs within 4096 units."));
    } else {
      var bar = obEl("div", "ob-bar");
      var grid = obEl("div", "ob-counts");
      for (var j = 0; j < OUTBREAK_STAGES.length; j++) {
        if (n[j] > 0) {
          var seg = obEl("span");
          seg.setAttribute("data-k", j);
          seg.style.flexGrow = n[j];
          seg.title = OUTBREAK_STAGES[j] + ": " + n[j];
          bar.appendChild(seg);
        }
        var cell = obEl("div");
        cell.setAttribute("data-k", j);
        cell.appendChild(obEl("b", n[j] ? "" : "zero", String(n[j])));
        cell.appendChild(obEl("small", "", OUTBREAK_STAGES[j]));
        grid.appendChild(cell);
      }
      nearby.appendChild(bar);
      nearby.appendChild(grid);
    }
  }
  var detector = document.getElementById("outbreakDetector");
  if (detector) {
    detector.textContent = "";
    detector.appendChild(obRow("Pathogen Scanner", data.scanner ? "Found" : "Missing", data.scanner ? 0 : 1, -1,
      data.scanner ? "Carry it or a Biometric Scanner to show sick markers." : "Scanner item not found (RadOutbreak.esp missing)"));
  }
};
