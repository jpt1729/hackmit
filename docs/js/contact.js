const STORAGE_KEY = "brain-buddy-caregiver-contact-v1";
const callLink = document.getElementById("caregiver-call");
const setupLink = document.getElementById("caregiver-setup-link");
const panel = document.querySelector(".caregiver-panel");
const form = document.getElementById("caregiver-form");
const nameInput = document.getElementById("caregiver-name");
const phoneInput = document.getElementById("caregiver-phone");
const status = document.getElementById("caregiver-save-status");

function validPhone(value) {
  const digits = value.replace(/\D/g, "");
  return /^\+?[\d().\s-]+$/.test(value) && digits.length >= 7 && digits.length <= 15;
}

function showContact(name, phone) {
  const dialNumber = (phone.startsWith("+") ? "+" : "") + phone.replace(/\D/g, "");
  callLink.textContent = `Call ${name}`;
  callLink.href = `tel:${dialNumber}`;
  callLink.hidden = false;
  setupLink.hidden = true;
  nameInput.value = name;
  phoneInput.value = phone;
}

function setupCaregiverContact() {
  try {
    const saved = JSON.parse(localStorage.getItem(STORAGE_KEY) || "null");
    if (saved && typeof saved.name === "string" && typeof saved.phone === "string" && validPhone(saved.phone)) showContact(saved.name, saved.phone);
  } catch { /* Contact can still be entered during this session. */ }
  setupLink.addEventListener("click", () => {
    document.dispatchEvent(new CustomEvent("demo-role-request", { detail: { role: "caregiver" } }));
    if (panel.hidden) return;
    panel.open = true;
    panel.scrollIntoView({ behavior: "smooth", block: "start" });
    nameInput.focus({ preventScroll: true });
  });
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    const name = nameInput.value.trim();
    const phone = phoneInput.value.trim();
    if (!name || !validPhone(phone)) {
      status.textContent = "Please enter a name and a phone number with 7 to 15 digits.";
      phoneInput.focus();
      return;
    }
    showContact(name, phone);
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify({ name, phone }));
      status.textContent = "Caregiver contact saved in this browser.";
    } catch {
      status.textContent = "Contact is ready for this visit. Browser storage is unavailable, so it may need to be entered again.";
    }
  });
}

export { setupCaregiverContact };
