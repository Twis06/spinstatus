const WASH_SECONDS = 120;
const IDLE_GRACE_SECONDS = 300;

const screen = document.querySelector("#screen");
const countdownValue = document.querySelector("#countdown-value");
const idleValue = document.querySelector("#idle-value");
const progressFill = document.querySelector("#progress-fill");
const cycleMeta = document.querySelector("#cycle-meta");
const fabricButton = document.querySelector("#fabric-button");
const remindButton = document.querySelector("#remind-button");
const idleWarning = document.querySelector("#idle-warning");
const captionStep = document.querySelector("#caption-step");
const captionText = document.querySelector("#caption-text");
const introOverlay = document.querySelector("#intro-overlay");
const recordButton = document.querySelector("#record-mode");
const stateButtons = [...document.querySelectorAll("[data-preview-state]")];
const views = [...document.querySelectorAll("[data-view]")];
const loadButtons = [...document.querySelectorAll("[data-load]")];

let state = "welcome";
let load = "MED";
let delicates = false;
let speed = 10;
let stateStartedAt = performance.now();
let remindTimeout = null;
let demoRun = 0;

const captions = {
  welcome: { step: "1 / 8", text: "Tap once to begin the washer check-in flow." },
  setup: { step: "2 / 8", text: "Choose load size and fabric mode before starting." },
  running: { step: "6 / 8", text: "The countdown stays large and easy to read." },
  idle: { step: "7 / 8", text: "Green means the load is done and ready for pickup." },
};

function wait(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function formatTime(seconds, prefix = "") {
  const safeSeconds = Math.max(0, Math.floor(seconds));
  const minutes = String(Math.floor(safeSeconds / 60)).padStart(2, "0");
  const remainder = String(safeSeconds % 60).padStart(2, "0");
  return `${prefix}${minutes}:${remainder}`;
}

function closeIntro() {
  document.body.classList.remove("intro-open");
  introOverlay.setAttribute("aria-hidden", "true");
}

function setCaption(caption) {
  const nextCaption =
    typeof caption === "string" ? { step: "DEMO", text: caption } : caption;
  captionStep.textContent = nextCaption.step;
  captionText.textContent = nextCaption.text;
}

function setLoad(nextLoad) {
  load = nextLoad;
  loadButtons.forEach((button) => button.classList.toggle("active", button.dataset.load === load));
}

function setDelicates(nextValue) {
  delicates = nextValue;
  fabricButton.textContent = delicates ? "DELICATE" : "NORMAL";
  fabricButton.classList.toggle("active", delicates);
}

function resetReminder() {
  remindButton.classList.remove("sent");
  remindButton.textContent = "REMIND";
  clearTimeout(remindTimeout);
}

function setIdlePhase(overdue) {
  screen.dataset.idlePhase = overdue ? "overdue" : "grace";
  idleWarning.textContent = overdue ? "GRACE PERIOD OVER - FAIR TO REMOVE" : "READY FOR PICKUP";
}

function setState(nextState, caption = captions[nextState]) {
  state = nextState;
  stateStartedAt = performance.now();
  screen.dataset.state = state;

  views.forEach((view) => {
    const active = view.dataset.view === state;
    view.classList.toggle("active", active);
    view.setAttribute("aria-hidden", String(!active));
  });

  stateButtons.forEach((button) => {
    button.classList.toggle("active", button.dataset.previewState === state);
  });

  resetReminder();
  setCaption(caption);

  if (state === "running") {
    countdownValue.textContent = formatTime(WASH_SECONDS);
    progressFill.style.width = "0%";
    cycleMeta.textContent = `${load} / ${delicates ? "DELICATE" : "NORMAL"}`;
  }

  if (state === "idle") {
    idleValue.textContent = formatTime(0, "+");
    setIdlePhase(false);
  } else {
    delete screen.dataset.idlePhase;
  }
}

function updateTimers(now) {
  const elapsedSeconds = ((now - stateStartedAt) / 1000) * speed;

  if (state === "running") {
    const remaining = WASH_SECONDS - elapsedSeconds;
    if (remaining <= 0) {
      setState("idle");
      return;
    }
    countdownValue.textContent = formatTime(Math.ceil(remaining));
    progressFill.style.width = `${Math.min(100, (elapsedSeconds / WASH_SECONDS) * 100)}%`;
  }

  if (state === "idle") {
    idleValue.textContent = formatTime(elapsedSeconds, "+");
    setIdlePhase(elapsedSeconds >= IDLE_GRACE_SECONDS);
  }
}

function frame(now) {
  updateTimers(now);
  requestAnimationFrame(frame);
}

async function runGuidedDemo() {
  const runId = ++demoRun;
  closeIntro();
  setLoad("MED");
  setDelicates(false);
  speed = 10;
  document.querySelector("#speed").value = "10";
  setState("welcome", {
    step: "1 / 8",
    text: "A simple touch starts the wildcard check-in.",
  });
  await wait(4600);
  if (runId !== demoRun) return;

  setState("setup", {
    step: "2 / 8",
    text: "The user lands on a minimal setup screen.",
  });
  await wait(4200);
  if (runId !== demoRun) return;
  setCaption({
    step: "3 / 8",
    text: "Select the load size for this wash.",
  });
  setLoad("LARGE");
  await wait(3800);
  if (runId !== demoRun) return;
  setCaption({
    step: "4 / 8",
    text: "Toggle delicate fabric mode when needed.",
  });
  setDelicates(true);
  await wait(3800);
  if (runId !== demoRun) return;
  setCaption({
    step: "5 / 8",
    text: "Start the selected cycle with one clear action.",
  });
  await wait(3400);
  if (runId !== demoRun) return;

  setState("running", {
    step: "6 / 8",
    text: "Start the wash and show time remaining.",
  });
  await wait(6200);
  if (runId !== demoRun) return;

  setState("idle", {
    step: "7 / 8",
    text: "Green shows the washer is done and ready to unload.",
  });
  await wait(5200);
  if (runId !== demoRun) return;

  stateStartedAt = performance.now() - (IDLE_GRACE_SECONDS + 33) * 1000 / speed;
  setIdlePhase(true);
  idleValue.textContent = formatTime(IDLE_GRACE_SECONDS + 33, "+");
  setCaption({
    step: "8 / 8",
    text: "After 5 minutes, red indicates the grace period is over.",
  });
  await wait(5600);
  if (runId !== demoRun) return;

  remindButton.textContent = "SENT";
  remindButton.classList.add("sent");
  setCaption({
    step: "8 / 8",
    text: "The longest overdue idle time makes removal fair for the next user.",
  });
  await wait(5200);
  if (runId !== demoRun) return;

  setState("welcome", {
    step: "1 / 8",
    text: "Collected clothes reset the panel for the next user.",
  });
}

document.querySelector("#enter-button").addEventListener("click", () => {
  closeIntro();
  setState("setup");
});

document.querySelector("#start-button").addEventListener("click", () => setState("running"));
document.querySelector("#finish-button").addEventListener("click", () => setState("idle"));
document.querySelector("#collected-button").addEventListener("click", () => setState("welcome"));
document.querySelector("#guided-demo").addEventListener("click", runGuidedDemo);
document.querySelector("#intro-play").addEventListener("click", runGuidedDemo);
document.querySelector("#intro-skip").addEventListener("click", closeIntro);

document.querySelector("#restart-demo").addEventListener("click", () => {
  demoRun++;
  setLoad("MED");
  setDelicates(false);
  setState("welcome");
});

recordButton.addEventListener("click", () => {
  document.body.classList.toggle("record-mode");
  recordButton.textContent = document.body.classList.contains("record-mode")
    ? "Exit record mode"
    : "Record mode";
});

document.querySelector("#caption-toggle").addEventListener("change", (event) => {
  document.body.classList.toggle("hide-captions", !event.target.checked);
});

fabricButton.addEventListener("click", () => setDelicates(!delicates));

remindButton.addEventListener("click", () => {
  remindButton.textContent = "SENT";
  remindButton.classList.add("sent");
  clearTimeout(remindTimeout);
  remindTimeout = setTimeout(resetReminder, 1800);
});

loadButtons.forEach((button) => {
  button.addEventListener("click", () => setLoad(button.dataset.load));
});

stateButtons.forEach((button) => {
  button.addEventListener("click", () => {
    demoRun++;
    closeIntro();
    setState(button.dataset.previewState);
  });
});

document.querySelector("#speed").addEventListener("change", (event) => {
  speed = Number(event.target.value);
  stateStartedAt = performance.now();
});

document.querySelector("#scale").addEventListener("input", (event) => {
  document.documentElement.style.setProperty("--preview-scale", event.target.value);
});

document.addEventListener("keydown", (event) => {
  if (event.key === "Escape") {
    closeIntro();
    document.body.classList.remove("record-mode");
    recordButton.textContent = "Record mode";
  }
});

window.spinstatusPreview = {
  setState,
  runGuidedDemo,
  setLoad,
  setDelicates,
};

setState("welcome");
requestAnimationFrame(frame);
