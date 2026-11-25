const state = {
  enabled: false,
  direction: 'CW',
  speed: 50
};

async function sendControl() {
  const enabledToggle = document.getElementById('enabled');
  const speedSlider = document.getElementById('speed');
  const speedPercent = Number(speedSlider.value);
  const speedSteps = Math.round(speedPercent * 20);

  state.enabled = enabledToggle.checked;
  state.speed = speedSteps;

  try {
    const response = await fetch('/', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        enabled: state.enabled,
        direction: state.direction,
        speed: speedSteps
      })
    });

    const result = await response.json();
    if (!result.success) {
      throw new Error(result.message || 'Control request failed');
    }
    updateStatus(result.data);
  } catch (error) {
    document.getElementById('status-text').textContent = 'Error: ' + error.message;
  }
}

function updateStatus(data) {
  if (!data) return;

  document.getElementById('enabled').checked = data.enabled;
  document.getElementById('status-text').textContent = data.enabled ? 'Running' : 'Stopped';
  document.getElementById('speed-value').textContent = document.getElementById('speed').value + '%';

  document.querySelectorAll('[data-direction]').forEach((button) => {
    button.classList.toggle('active', button.dataset.direction === data.direction);
  });
}

async function moveDegrees() {
  const degreesInput = document.getElementById('degrees');
  const speedSlider = document.getElementById('speed');
  const degrees = Number(degreesInput.value);
  const speed = Math.round(Number(speedSlider.value) * 20);

  if (!Number.isFinite(degrees) || degrees <= 0 || degrees > 360) {
    document.getElementById('status-text').textContent = 'Degrees must be between 1.8 and 360';
    return;
  }

  try {
    const response = await fetch('/move/degrees', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ degrees, direction: state.direction, speed })
    });

    const result = await response.json();
    if (!result.success) {
      throw new Error(result.message || 'Movement request failed');
    }
    document.getElementById('status-text').textContent = 'Moving ' + degrees + '° ' + state.direction;
  } catch (error) {
    document.getElementById('status-text').textContent = 'Error: ' + error.message;
  }
}

async function refreshStatus() {
  try {
    const response = await fetch('/status');
    const result = await response.json();
    if (result.success) {
      const data = result.data;
      state.enabled = data.enabled;
      state.direction = data.direction;

      const enabledToggle = document.getElementById('enabled');
      enabledToggle.checked = data.enabled;

      const percent = Math.round(data.speed / 20);
      const speedSlider = document.getElementById('speed');
      speedSlider.value = percent;
      document.getElementById('speed-value').textContent = percent + '%';
      document.getElementById('status-text').textContent = data.enabled ? 'Running' : 'Stopped';

      document.querySelectorAll('[data-direction]').forEach((button) => {
        button.classList.toggle('active', button.dataset.direction === data.direction);
      });
    }
  } catch (error) {
    document.getElementById('status-text').textContent = 'Connection error';
  }
}

window.addEventListener('DOMContentLoaded', () => {
  const enabledToggle = document.getElementById('enabled');
  const speedSlider = document.getElementById('speed');

  enabledToggle.addEventListener('change', sendControl);

  speedSlider.addEventListener('input', () => {
    document.getElementById('speed-value').textContent = speedSlider.value + '%';
  });
  speedSlider.addEventListener('change', sendControl);

  document.querySelectorAll('[data-direction]').forEach((button) => {
    button.addEventListener('click', () => {
      state.direction = button.dataset.direction;
      document.querySelectorAll('[data-direction]').forEach((item) => {
        item.classList.toggle('active', item === button);
      });
      sendControl();
    });
  });

  document.getElementById('move-degrees').addEventListener('click', moveDegrees);

  refreshStatus();
  setInterval(refreshStatus, 3000);
});
