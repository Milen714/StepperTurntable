/**
 * Motor Control Module
 * Handles slider and button interactions for blind control
 */

class MotorControl {
  constructor() {
    this.isMoving = false;
    this.currentDirection = 'CW';
    this.currentSteps = 0;
  }

  /**
   * Initialize slider control for index page
   */
  initSlider() {
    const interactionArea = document.getElementById('slider-interaction');
    const thumb = document.getElementById('slider-thumb');
    const fill = document.getElementById('slider-fill');
    const display = document.getElementById('percentage-display');
    const container = document.getElementById('slider-container');

    let isDragging = false;
    let lastPercentage = parseInt(display.innerText);

    const updateSlider = (y) => {
      const rect = container.getBoundingClientRect();
      let percentage = ((rect.bottom - y) / rect.height) * 100;
      percentage = Math.max(0, Math.min(100, percentage));

      const rounded = Math.round(percentage);
      display.innerText = rounded;

      // Update UI
      fill.style.height = `${percentage}%`;
      thumb.style.bottom = `calc(${percentage}% - 36px)`;
    };

    const submitSliderMovement = (newPercentage) => {
      const stepsDifference = Math.abs(newPercentage - lastPercentage);
      if (stepsDifference === 0) return;

      const direction = newPercentage > lastPercentage ? 'CW' : 'CCW';
      const steps = stepsDifference * 100; // Convert percentage to approximate steps

      this.disableSlider(interactionArea);
      this.submitMovement(direction, steps, () => {
        lastPercentage = newPercentage;
        this.enableSlider(interactionArea);
      });
    };

    // Fetch current position on page load
    this.fetchAndInitializePosition().then((position) => {
      // Update lastPercentage to the current position percentage
      lastPercentage = position;
    });

    interactionArea.addEventListener('mousedown', (e) => {
      if (this.isMoving) return;
      isDragging = true;
      updateSlider(e.clientY);
    });

    window.addEventListener('mousemove', (e) => {
      if (isDragging && !this.isMoving) {
        updateSlider(e.clientY);
      }
    });

    window.addEventListener('mouseup', () => {
      if (isDragging) {
        isDragging = false;
        const newPercentage = parseInt(display.innerText);
        submitSliderMovement(newPercentage);
      }
    });

    // Touch support
    interactionArea.addEventListener('touchstart', (e) => {
      if (this.isMoving) return;
      isDragging = true;
      updateSlider(e.touches[0].clientY);
      e.preventDefault();
    }, { passive: false });

    window.addEventListener('touchmove', (e) => {
      if (isDragging && !this.isMoving) {
        updateSlider(e.touches[0].clientY);
        e.preventDefault();
      }
    }, { passive: false });

    window.addEventListener('touchend', () => {
      if (isDragging) {
        isDragging = false;
        const newPercentage = parseInt(document.getElementById('percentage-display').innerText);
        submitSliderMovement(newPercentage);
      }
    });
  }

  /**
   * Initialize precise controls
   */
  initPreciseControls() {
    const dirDownBtn = document.getElementById('dir-down');
    const dirUpBtn = document.getElementById('dir-up');
    const stepsInput = document.getElementById('steps-input');
    const goButton = document.getElementById('go-button');

    if (dirDownBtn) {
      dirDownBtn.addEventListener('click', () => {
        this.currentDirection = 'CW';
        dirDownBtn.classList.add('active');
        dirUpBtn.classList.remove('active');
      });
    }

    if (dirUpBtn) {
      dirUpBtn.addEventListener('click', () => {
        this.currentDirection = 'CCW';
        dirUpBtn.classList.add('active');
        dirDownBtn.classList.remove('active');
      });
    }

    if (goButton) {
      goButton.addEventListener('click', () => {
        if (this.isMoving) return;

        const steps = parseInt(stepsInput.value);
        if (!steps || steps <= 0) {
          alert('Please enter a valid number of steps.');
          return;
        }

        this.disableControls(stepsInput, goButton);
        this.submitMovement(this.currentDirection, steps, () => {
          this.enableControls(stepsInput, goButton);
          stepsInput.value = '';
        });
      });
    }
  }

  /**
   * Submit movement to the API
   */
  submitMovement(direction, steps, callback) {
    this.isMoving = true;
    const direction_map = direction === 'CW' || direction === 'Down' ? 'CW' : 'CCW';

    const payload = {
      direction: direction_map,
      steps: Math.round(steps)
    };

    console.log('Submitting:', payload);

    fetch('/', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(payload)
    })
      .then((response) => response.json())
      .then((data) => {
        console.log('Movement started:', data);
        
        // Poll status endpoint until motor stops
        this.pollStatus(callback);
      })
      .catch((error) => {
        console.error('Error:', error);
        alert('Failed to move blinds. Please try again.');
        this.isMoving = false;
        if (callback) callback();
      });
  }

  /**
   * Poll status endpoint until motor stops
   */
  pollStatus(callback) {
    const pollInterval = setInterval(() => {
      fetch('/status')
        .then((response) => response.json())
        .then((data) => {
          console.log('Status:', data);
          
          const isMoving = data.data && data.data.moving;
          const position = data.data && data.data.position;
          
          // Update position display if it exists
          if (position !== undefined) {
            const positionDisplay = document.querySelector('.position-value');
            if (positionDisplay) {
              positionDisplay.innerText = `Position: ${position}`;
            }
          }
          
          // Stop polling when motor is no longer moving
          if (!isMoving) {
            clearInterval(pollInterval);
            console.log('Motor stopped at position:', position);
            this.isMoving = false;
            if (callback) callback();
          }
        })
        .catch((error) => {
          console.error('Error polling status:', error);
        });
    }, 2000); // Poll every 200ms
  }

  /**
   * Disable slider during movement
   */
  disableSlider(interactionArea) {
    interactionArea.style.pointerEvents = 'none';
    interactionArea.style.opacity = '0.5';
  }

  /**
   * Enable slider after movement
   */
  enableSlider(interactionArea) {
    interactionArea.style.pointerEvents = 'auto';
    interactionArea.style.opacity = '1';
  }

  /**
   * Disable precise control inputs
   */
  disableControls(stepsInput, goButton) {
    stepsInput.disabled = true;
    goButton.disabled = true;
    goButton.style.opacity = '0.6';
    stepsInput.style.opacity = '0.6';
  }

  /**
   * Enable precise control inputs
   */
  enableControls(stepsInput, goButton) {
    stepsInput.disabled = false;
    goButton.disabled = false;
    goButton.style.opacity = '1';
    stepsInput.style.opacity = '1';
  }

  /**
   * Fetch current position from backend and update slider
   */
  async fetchAndInitializePosition() {
    try {
      const response = await fetch('/status');
      const data = await response.json();

      if (data.data && data.data.position !== undefined) {
        let position = data.data.position;
        console.log('Current position (raw):', position);

        // Convert position to percentage (0-100)
        // Assuming max position is 10000 steps (or adjust based on your motor range)
        const MAX_POSITION = 10000;
        const percentage = Math.round((Math.abs(position) / MAX_POSITION) * 100);
        
        // Clamp to 0-100
        const clampedPercentage = Math.max(0, Math.min(100, percentage));
        console.log('Position percentage:', clampedPercentage);

        // Update slider UI to reflect current position
        const display = document.getElementById('percentage-display');
        const fill = document.getElementById('slider-fill');
        const thumb = document.getElementById('slider-thumb');

        if (display && fill && thumb) {
          display.innerText = clampedPercentage;
          fill.style.height = `${clampedPercentage}%`;
          thumb.style.bottom = `calc(${clampedPercentage}% - 36px)`;
          
          console.log('Slider initialized to position:', clampedPercentage + '%');
        }

        return clampedPercentage;
      }
    } catch (error) {
      console.error('Error fetching initial position:', error);
      // Default to stored value if fetch fails
      return parseInt(document.getElementById('percentage-display').innerText);
    }
  }

  /**
   * Fetch and update position display on controls page
   */
  async updatePositionDisplay() {
    try {
      const response = await fetch('/status');
      const data = await response.json();

      if (data.data && data.data.position !== undefined) {
        const position = data.data.position;
        const posDisplay = document.querySelector('.position-value');
        
        if (posDisplay) {
          posDisplay.textContent = `Position: ${position}`;
          console.log('Position display updated:', position);
        }
      }
    } catch (error) {
      console.error('Error updating position display:', error);
    }
  }
}

/**
 * Reset position to zero
 * Calls /reset endpoint to zero the position counter
 */
async function resetPosition() {
  try {
    const response = await fetch('/reset');
    const data = await response.json();
    
    if (data.success) {
      console.log('Position reset successfully');
      
      // Update slider display if on index page
      const display = document.getElementById('percentage-display');
      const fill = document.getElementById('slider-fill');
      const thumb = document.getElementById('slider-thumb');
      
      if (display) display.innerText = '0';
      if (fill) fill.style.height = '0%';
      if (thumb) thumb.style.bottom = 'calc(0% - 36px)';
      
      // Update position display if on controls page
      const posDisplay = document.querySelector('.position-value');
      if (posDisplay) posDisplay.textContent = 'Position: 0';
    } else {
      console.error('Failed to reset position:', data.message);
    }
  } catch (error) {
    console.error('Error resetting position:', error);
  }
}

// Initialize on page load
window.addEventListener('DOMContentLoaded', () => {
  const motorControl = new MotorControl();

  // Check which controls exist on this page
  if (document.getElementById('slider-interaction')) {
    motorControl.initSlider();
  }

  if (document.getElementById('go-button')) {
    motorControl.initPreciseControls();
  }

  // Update position display if on controls page
  if (document.querySelector('.position-value')) {
    motorControl.updatePositionDisplay();
  }
});
