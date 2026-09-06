/**
 * PUMP MY FAMILY - Frontend Dashboard Application
 */

const API_BASE = '/api';

// State
let token = localStorage.getItem('pump_token') || null;
let currentUser = null;
let devicesList = [];
let currentDevice = null;
let pollingInterval = null;

// DOM Elements
const authModal = document.getElementById('authModal');
const addDeviceModal = document.getElementById('addDeviceModal');
const shareDeviceModal = document.getElementById('shareDeviceModal');
const openAuthModalBtn = document.getElementById('openAuthModalBtn');
const userMenuArea = document.getElementById('userMenuArea');
const displayUserName = document.getElementById('displayUserName');
const displayUserRole = document.getElementById('displayUserRole');
const logoutBtn = document.getElementById('logoutBtn');
const mainDashboard = document.getElementById('mainDashboard');
const welcomeScreen = document.getElementById('welcomeScreen');
const welcomeLoginBtn = document.getElementById('welcomeLoginBtn');
const systemStatusBadge = document.getElementById('systemStatusBadge');

// Auth Form Elements
const tabLogin = document.getElementById('tabLogin');
const tabRegister = document.getElementById('tabRegister');
const loginForm = document.getElementById('loginForm');
const registerForm = document.getElementById('registerForm');
const closeAuthModal = document.getElementById('closeAuthModal');

// Device & Controls
const deviceSelector = document.getElementById('deviceSelector');
const currentDeviceCode = document.getElementById('currentDeviceCode');
const currentDeviceRoleBadge = document.getElementById('currentDeviceRoleBadge');
const btnOpenShareModal = document.getElementById('btnOpenShareModal');
const btnRefresh = document.getElementById('btnRefresh');
const btnAddDevice = document.getElementById('btnAddDevice');
const closeAddDeviceModal = document.getElementById('closeAddDeviceModal');
const addDeviceForm = document.getElementById('addDeviceForm');
const closeShareModal = document.getElementById('closeShareModal');
const shareDeviceForm = document.getElementById('shareDeviceForm');

// Tank Config
const btnOpenTankConfigModal = document.getElementById('btnOpenTankConfigModal');
const tankConfigModal = document.getElementById('tankConfigModal');
const closeTankConfigModal = document.getElementById('closeTankConfigModal');
const tankConfigForm = document.getElementById('tankConfigForm');
const cfgTankHeight = document.getElementById('cfgTankHeight');
const cfgTankOffset = document.getElementById('cfgTankOffset');
const cfgMinWaterPct = document.getElementById('cfgMinWaterPct');
const cfgMaxWaterPct = document.getElementById('cfgMaxWaterPct');

// Admin Portal Elements
const btnAdminPortal = document.getElementById('btnAdminPortal');
const adminPortalModal = document.getElementById('adminPortalModal');
const closeAdminModal = document.getElementById('closeAdminModal');
const tabAdminUsers = document.getElementById('tabAdminUsers');
const tabAdminOta = document.getElementById('tabAdminOta');
const adminUsersSection = document.getElementById('adminUsersSection');
const adminOtaSection = document.getElementById('adminOtaSection');
const adminUsersTableBody = document.getElementById('adminUsersTableBody');
const btnAdminAddUser = document.getElementById('btnAdminAddUser');

// Admin User Modal
const adminUserModal = document.getElementById('adminUserModal');
const closeAdminUserModal = document.getElementById('closeAdminUserModal');
const adminUserForm = document.getElementById('adminUserForm');
const adminUserModalTitle = document.getElementById('adminUserModalTitle');
const adminUserId = document.getElementById('adminUserId');
const adminUserFullName = document.getElementById('adminUserFullName');
const adminUserEmail = document.getElementById('adminUserEmail');
const adminUserRole = document.getElementById('adminUserRole');
const adminUserPassword = document.getElementById('adminUserPassword');
const adminUserPassLabel = document.getElementById('adminUserPassLabel');

// OTA Elements
const adminOtaForm = document.getElementById('adminOtaForm');
const otaTargetSelect = document.getElementById('otaTargetSelect');
const otaVersionInput = document.getElementById('otaVersionInput');
const otaBinFileInput = document.getElementById('otaBinFileInput');
const otaFileNameDisplay = document.getElementById('otaFileNameDisplay');
const otaStatusBox = document.getElementById('otaStatusBox');
const otaStatusText = document.getElementById('otaStatusText');
const otaSpinner = document.getElementById('otaSpinner');
const btnSubmitOta = document.getElementById('btnSubmitOta');
const adminFirmwareTableBody = document.getElementById('adminFirmwareTableBody');

// Tank & Telemetry
const waterBody = document.getElementById('waterBody');
const waterPercentDisplay = document.getElementById('waterPercentDisplay');
const waterStatusText = document.getElementById('waterStatusText');
const nodeBatteryDisplay = document.getElementById('nodeBatteryDisplay');
const waterDistanceDisplay = document.getElementById('waterDistanceDisplay');
const pumpRuntimeDisplay = document.getElementById('pumpRuntimeDisplay');
const pumpStatePill = document.getElementById('pumpStatePill');

// Controls
const pumpToggleButton = document.getElementById('pumpToggleButton');
const pumpButtonText = document.getElementById('pumpButtonText');
const toggleAutoMode = document.getElementById('toggleAutoMode');
const toggleChildLock = document.getElementById('toggleChildLock');
const viewOnlyAlert = document.getElementById('viewOnlyAlert');
const logsTableBody = document.getElementById('logsTableBody');

// Toast
const appToast = document.getElementById('appToast');
const toastMessage = document.getElementById('toastMessage');

// ==================== INITIALIZATION ====================
document.addEventListener('DOMContentLoaded', () => {
  initEventListeners();
  checkAuth();
});

function initEventListeners() {
  // Modal openers
  openAuthModalBtn.addEventListener('click', () => openModal(authModal));
  welcomeLoginBtn.addEventListener('click', () => openModal(authModal));
  closeAuthModal.addEventListener('click', () => closeModal(authModal));
  
  btnAddDevice.addEventListener('click', () => openModal(addDeviceModal));
  closeAddDeviceModal.addEventListener('click', () => closeModal(addDeviceModal));

  btnOpenShareModal.addEventListener('click', () => openModal(shareDeviceModal));
  closeShareModal.addEventListener('click', () => closeModal(shareDeviceModal));

  // Tabs
  tabLogin.addEventListener('click', () => {
    tabLogin.classList.add('active');
    tabRegister.classList.remove('active');
    loginForm.style.display = 'block';
    registerForm.style.display = 'none';
  });
  tabRegister.addEventListener('click', () => {
    tabRegister.classList.add('active');
    tabLogin.classList.remove('active');
    loginForm.style.display = 'none';
    registerForm.style.display = 'block';
  });

  // Auth Forms
  loginForm.addEventListener('submit', handleLogin);
  registerForm.addEventListener('submit', handleRegister);
  logoutBtn.addEventListener('click', handleLogout);

  // Sidebar Navigation Items
  const navHome = document.getElementById('navHome');
  const navAdmin = document.getElementById('navAdmin');
  const navLogs = document.getElementById('navLogs');
  const navOpenTankConfig = document.getElementById('navOpenTankConfig');
  const navAddDevice = document.getElementById('navAddDevice');
  const btnBackToHome = document.getElementById('btnBackToHome');
  const btnRefreshTop = document.getElementById('btnRefreshTop');

  if (navHome) navHome.addEventListener('click', () => switchMainView('dashboard'));
  if (navAdmin) navAdmin.addEventListener('click', () => switchMainView('admin'));
  if (navLogs) navLogs.addEventListener('click', () => switchMainView('logs'));
  if (navOpenTankConfig) navOpenTankConfig.addEventListener('click', handleOpenTankConfig);
  if (navAddDevice) navAddDevice.addEventListener('click', () => openModal(addDeviceModal));
  if (btnBackToHome) btnBackToHome.addEventListener('click', () => switchMainView('dashboard'));
  if (btnRefreshTop) btnRefreshTop.addEventListener('click', () => fetchDeviceData(true));

  // Sidebar Drawer Mobile Toggles
  const btnToggleSidebar = document.getElementById('btnToggleSidebar');
  const btnCloseSidebar = document.getElementById('btnCloseSidebar');
  const appSidebar = document.getElementById('appSidebar');
  const sidebarBackdrop = document.getElementById('sidebarBackdrop');

  if (btnToggleSidebar && appSidebar) {
    btnToggleSidebar.addEventListener('click', () => {
      appSidebar.classList.toggle('active');
      if (sidebarBackdrop) sidebarBackdrop.classList.toggle('active');
    });
  }
  if (btnCloseSidebar && appSidebar) {
    btnCloseSidebar.addEventListener('click', () => {
      appSidebar.classList.remove('active');
      if (sidebarBackdrop) sidebarBackdrop.classList.remove('active');
    });
  }
  if (sidebarBackdrop && appSidebar) {
    sidebarBackdrop.addEventListener('click', () => {
      appSidebar.classList.remove('active');
      sidebarBackdrop.classList.remove('active');
    });
  }

  // Device Actions
  deviceSelector.addEventListener('change', handleDeviceChange);
  btnRefresh.addEventListener('click', () => fetchDeviceData(true));
  addDeviceForm.addEventListener('submit', handleAddDevice);
  shareDeviceForm.addEventListener('submit', handleShareDevice);

  // Tank Config Actions
  btnOpenTankConfigModal.addEventListener('click', handleOpenTankConfig);
  closeTankConfigModal.addEventListener('click', () => closeModal(tankConfigModal));
  tankConfigForm.addEventListener('submit', handleSaveTankConfig);

  // Admin Portal Actions
  if (btnAdminPortal) btnAdminPortal.addEventListener('click', handleOpenAdminPortal);
  if (closeAdminModal && adminPortalModal) closeAdminModal.addEventListener('click', () => closeModal(adminPortalModal));
  if (tabAdminUsers) tabAdminUsers.addEventListener('click', () => switchAdminTab('users'));
  if (tabAdminOta) tabAdminOta.addEventListener('click', () => switchAdminTab('ota'));

  btnAdminAddUser.addEventListener('click', handleOpenAddUserModal);
  closeAdminUserModal.addEventListener('click', () => closeModal(adminUserModal));
  adminUserForm.addEventListener('submit', handleSaveAdminUser);

  // OTA DropZone & Form
  otaBinFileInput.addEventListener('change', (e) => {
    if (e.target.files.length > 0) {
      otaFileNameDisplay.textContent = `File đã chọn: ${e.target.files[0].name} (${(e.target.files[0].size / 1024).toFixed(1)} KB)`;
      otaFileNameDisplay.style.color = '#38bdf8';
    }
  });
  adminOtaForm.addEventListener('submit', handleTriggerOta);

  // Pump Controls
  pumpToggleButton.addEventListener('click', handleTogglePump);
  toggleAutoMode.addEventListener('change', handleToggleAutoMode);
  toggleChildLock.addEventListener('change', handleToggleChildLock);
}

// ==================== API HELPER ====================
async function apiCall(endpoint, method = 'GET', body = null) {
  const headers = { 'Content-Type': 'application/json' };
  if (token) {
    headers['Authorization'] = `Bearer ${token}`;
  }

  try {
    const res = await fetch(`${API_BASE}${endpoint}`, {
      method,
      headers,
      body: body ? JSON.stringify(body) : null
    });

    const data = await res.json().catch(() => ({}));
    if (!res.ok) {
      throw new Error(data.detail || data.error || 'Có lỗi xảy ra!');
    }
    return data;
  } catch (err) {
    console.error(`API Error [${endpoint}]:`, err);
    throw err;
  }
}

// ==================== AUTHENTICATION ====================
async function checkAuth() {
  if (!token) {
    showLoggedOutState();
    return;
  }

  try {
    const res = await apiCall('/auth/me');
    currentUser = res;
    showLoggedInState();
    await loadDevices();
    startPolling();
  } catch (err) {
    handleLogout();
  }
}

function showLoggedInState() {
  openAuthModalBtn.style.display = 'none';
  userMenuArea.style.display = 'flex';
  welcomeScreen.style.display = 'none';

  displayUserName.textContent = currentUser.full_name || currentUser.email;
  displayUserRole.textContent = currentUser.role || 'USER';

  const navAdmin = document.getElementById('navAdmin');
  const isAdmin = (currentUser.role === 'ADMIN');
  if (navAdmin) navAdmin.style.display = isAdmin ? 'flex' : 'none';
  if (btnAdminPortal) btnAdminPortal.style.display = isAdmin ? 'inline-flex' : 'none';

  switchMainView('dashboard');
}

function showLoggedOutState() {
  token = null;
  currentUser = null;
  localStorage.removeItem('pump_token');
  stopPolling();

  openAuthModalBtn.style.display = 'inline-flex';
  userMenuArea.style.display = 'none';
  welcomeScreen.style.display = 'flex';
  mainDashboard.style.display = 'none';

  const viewAdminPortal = document.getElementById('viewAdminPortal');
  if (viewAdminPortal) viewAdminPortal.style.display = 'none';

  const navAdmin = document.getElementById('navAdmin');
  if (navAdmin) navAdmin.style.display = 'none';
  if (btnAdminPortal) btnAdminPortal.style.display = 'none';

  systemStatusBadge.className = 'status-badge offline';
  systemStatusBadge.querySelector('.status-label').textContent = 'Chưa đăng nhập';
}

async function handleLogin(e) {
  e.preventDefault();
  const email = document.getElementById('loginEmail').value.trim();
  const password = document.getElementById('loginPassword').value;

  try {
    const data = await apiCall('/auth/login', 'POST', { email, password });
    token = data.access_token;
    localStorage.setItem('pump_token', token);
    currentUser = data.user;
    closeModal(authModal);
    showToast('Đăng nhập thành công!', 'success');
    showLoggedInState();
    await loadDevices();
    startPolling();
  } catch (err) {
    showToast(err.message, 'error');
  }
}

async function handleRegister(e) {
  e.preventDefault();
  const full_name = document.getElementById('regFullName').value.trim();
  const email = document.getElementById('regEmail').value.trim();
  const password = document.getElementById('regPassword').value;

  try {
    await apiCall('/auth/register', 'POST', { email, password, full_name });
    showToast('Đăng ký tài khoản thành công! Hãy đăng nhập.', 'success');
    tabLogin.click();
    document.getElementById('loginEmail').value = email;
  } catch (err) {
    showToast(err.message, 'error');
  }
}

function handleLogout() {
  showLoggedOutState();
  showToast('Đã đăng xuất tài khoản!', 'success');
}

// ==================== DEVICES & POLLING ====================
async function loadDevices() {
  try {
    const res = await apiCall('/devices/');
    devicesList = [
      ...(res.owned_devices || []),
      ...(res.shared_devices || [])
    ];

    deviceSelector.innerHTML = '';

    if (devicesList.length === 0) {
      deviceSelector.innerHTML = '<option value="">Chưa có máy bơm nào (Bấm Thêm Bơm)</option>';
      currentDevice = null;
      resetDashboardView();
      return;
    }

    devicesList.forEach((d) => {
      const opt = document.createElement('option');
      opt.value = d.id;
      opt.textContent = `${d.name} (${d.role === 'OWNER' ? 'Chủ sở hữu' : 'Được chia sẻ'})`;
      deviceSelector.appendChild(opt);
    });

    // Select first device if not selected
    if (!currentDevice || !devicesList.find(d => d.id === currentDevice.id)) {
      currentDevice = devicesList[0];
      deviceSelector.value = currentDevice.id;
    }

    updateCurrentDeviceView();
    await fetchDeviceLogs();
  } catch (err) {
    console.error('Lỗi tải thiết bị:', err);
  }
}

function handleDeviceChange(e) {
  const selectedId = e.target.value;
  currentDevice = devicesList.find(d => d.id === selectedId) || null;
  updateCurrentDeviceView();
  fetchDeviceLogs();
}

function updateCurrentDeviceView() {
  if (!currentDevice) {
    resetDashboardView();
    return;
  }

  // Meta Info
  currentDeviceCode.textContent = currentDevice.device_code;
  currentDeviceRoleBadge.textContent = currentDevice.role;
  currentDeviceRoleBadge.className = `badge ${currentDevice.role === 'OWNER' ? '' : 'badge-member'}`;
  btnOpenShareModal.style.display = currentDevice.role === 'OWNER' ? 'inline-flex' : 'none';

  const currentFwVersion = document.getElementById('currentFwVersion');
  const currentTankFwVersion = document.getElementById('currentTankFwVersion');
  if (currentFwVersion) {
    currentFwVersion.textContent = `v${currentDevice.firmware_version || '1.0.0'}`;
  }
  if (currentTankFwVersion) {
    currentTankFwVersion.textContent = `v${currentDevice.tank_firmware_version || '1.0.0'}`;
  }
  updateOtaTargetVersionDisplay();

  // Online / Offline Status
  if (currentDevice.is_online) {
    systemStatusBadge.className = 'status-badge online';
    systemStatusBadge.querySelector('.status-label').textContent = 'ONLINE (Cloud)';
  } else {
    systemStatusBadge.className = 'status-badge offline';
    systemStatusBadge.querySelector('.status-label').textContent = 'OFFLINE (Mất kết nối)';
  }

  // Water Level & Color Dynamics
  const level = Math.max(0, Math.min(100, currentDevice.water_level || 0));
  waterBody.style.height = `${level}%`;
  waterPercentDisplay.textContent = `${level}%`;

  if (level < 20) {
    waterBody.style.background = 'linear-gradient(180deg, #ef4444 0%, #b91c1c 100%)';
    waterStatusText.textContent = '⚠️ BỂ CẠN NƯỚC';
    waterStatusText.style.color = '#fca5a5';
  } else if (level < 60) {
    waterBody.style.background = 'linear-gradient(180deg, #f59e0b 0%, #d97706 100%)';
    waterStatusText.textContent = 'Mức Nước Trung Bình';
    waterStatusText.style.color = '#fde68a';
  } else {
    waterBody.style.background = 'var(--water-gradient)';
    waterStatusText.textContent = level >= 90 ? '💧 BỂ ĐẦY NƯỚC' : 'Mức Nước Tốt';
    waterStatusText.style.color = '#a7f3d0';
  }

  // Pump State Button
  if (currentDevice.is_pump_running) {
    pumpToggleButton.classList.add('active');
    pumpButtonText.textContent = 'BƠM ĐANG CHẠY';
    pumpStatePill.textContent = 'ĐANG BƠM';
    pumpStatePill.className = 'device-state-pill state-running';
  } else {
    pumpToggleButton.classList.remove('active');
    pumpButtonText.textContent = 'MÁY BƠM TẮT';
    pumpStatePill.textContent = 'CHỜ BƠM';
    pumpStatePill.className = 'device-state-pill state-idle';
  }

  // Toggles without triggering change events
  toggleAutoMode.checked = !!currentDevice.is_auto_mode;
  toggleChildLock.checked = !!currentDevice.is_child_lock;

  // Permissions Enforcement
  const isViewOnly = (currentDevice.role === 'MEMBER' && currentDevice.permission === 'VIEW_ONLY');
  if (isViewOnly) {
    viewOnlyAlert.style.display = 'flex';
    pumpToggleButton.classList.add('disabled');
    toggleAutoMode.disabled = true;
    toggleChildLock.disabled = true;
  } else {
    viewOnlyAlert.style.display = 'none';
    pumpToggleButton.classList.remove('disabled');
    toggleAutoMode.disabled = false;
    toggleChildLock.disabled = false;
  }
}

function resetDashboardView() {
  waterBody.style.height = '0%';
  waterPercentDisplay.textContent = '--%';
  waterStatusText.textContent = 'Chưa có thiết bị';
  currentDeviceCode.textContent = '---';
}

async function fetchDeviceData(showToastNotice = false) {
  if (!token) return;
  try {
    const res = await apiCall('/devices/');
    devicesList = [
      ...(res.owned_devices || []),
      ...(res.shared_devices || [])
    ];

    if (currentDevice) {
      const updated = devicesList.find(d => d.id === currentDevice.id);
      if (updated) {
        // Thông báo nổi bật nếu phát hiện thiết bị vừa nâng cấp firmware xong
        if (currentDevice.firmware_version && updated.firmware_version &&
            currentDevice.firmware_version !== updated.firmware_version) {
          showToast(`🎉 TỦ ĐIỆN ĐÃ NÂNG CẤP THÀNH CÔNG LÊN FIRMWARE: v${updated.firmware_version}!`, 'success');
        }
        if (currentDevice.tank_firmware_version && updated.tank_firmware_version &&
            currentDevice.tank_firmware_version !== updated.tank_firmware_version) {
          showToast(`🎉 BỂ NƯỚC ĐÃ NÂNG CẤP THÀNH CÔNG LÊN FIRMWARE: v${updated.tank_firmware_version}!`, 'success');
        }
        currentDevice = updated;
        updateCurrentDeviceView();
      }
    }
    if (showToastNotice) showToast('Đã làm mới dữ liệu!', 'success');
  } catch (err) {
    console.error('Polling error:', err);
  }
}

async function fetchDeviceLogs() {
  if (!currentDevice) return;
  try {
    const data = await apiCall(`/pumps/${currentDevice.id}/logs`);
    const logs = data.logs || [];
    renderLogs(logs);
  } catch (err) {
    console.error('Lỗi tải nhật ký:', err);
  }
}

function renderLogs(logs) {
  if (logs.length === 0) {
    logsTableBody.innerHTML = `<tr><td colspan="4" class="text-center py-4 text-muted">Chưa có nhật ký hoạt động nào.</td></tr>`;
    return;
  }

  logsTableBody.innerHTML = logs.map(l => {
    const timeStr = new Date(l.created_at).toLocaleString('vi-VN');
    const userStr = typeof l.triggered_by === 'object' && l.triggered_by 
      ? `${l.triggered_by.full_name} (${l.triggered_by.email})` 
      : (l.triggered_by || 'Tự Động (Auto)');
    
    let actionBadge = `<span class="badge">${l.action}</span>`;
    if (l.action === 'PUMP_ON') actionBadge = `<span class="badge" style="background:rgba(16,185,129,0.2); color:#34d399; border-color:#10b981;">BẬT BƠM</span>`;
    if (l.action === 'PUMP_OFF') actionBadge = `<span class="badge" style="background:rgba(239,68,68,0.2); color:#f87171; border-color:#ef4444;">TẮT BƠM</span>`;

    return `
      <tr>
        <td>${timeStr}</td>
        <td>${actionBadge}</td>
        <td>${userStr}</td>
        <td class="font-mono">${l.water_level !== null ? l.water_level + '%' : '--'}</td>
      </tr>
    `;
  }).join('');
}

function startPolling() {
  stopPolling();
  pollingInterval = setInterval(() => {
    fetchDeviceData(false);
  }, 2500); // 2.5 giây cập nhật 1 lần
}

function stopPolling() {
  if (pollingInterval) {
    clearInterval(pollingInterval);
    pollingInterval = null;
  }
}

// ==================== PUMP CONTROL ACTIONS ====================
async function handleTogglePump() {
  if (!currentDevice) return;
  const nextAction = currentDevice.is_pump_running ? 'PUMP_OFF' : 'PUMP_ON';
  
  try {
    showToast(`Đang gửi lệnh ${nextAction}...`, 'success');
    await apiCall(`/pumps/${currentDevice.id}/control`, 'POST', { action: nextAction });
    // Tạm thời đảo trạng thái để UI mượt mà
    currentDevice.is_pump_running = !currentDevice.is_pump_running;
    updateCurrentDeviceView();
    setTimeout(() => {
      fetchDeviceData(false);
      fetchDeviceLogs();
    }, 1000);
  } catch (err) {
    showToast(err.message, 'error');
  }
}

async function handleToggleAutoMode(e) {
  if (!currentDevice) return;
  const action = e.target.checked ? 'AUTO_MODE' : 'MANUAL_MODE';
  try {
    await apiCall(`/pumps/${currentDevice.id}/control`, 'POST', { action });
    showToast(`Đã chuyển sang ${action === 'AUTO_MODE' ? 'Chế độ Tự Động' : 'Chế độ Bằng Tay'}!`, 'success');
  } catch (err) {
    e.target.checked = !e.target.checked;
    showToast(err.message, 'error');
  }
}

async function handleToggleChildLock(e) {
  if (!currentDevice) return;
  const action = e.target.checked ? 'CHILD_LOCK_ON' : 'CHILD_LOCK_OFF';
  try {
    await apiCall(`/pumps/${currentDevice.id}/control`, 'POST', { action });
    showToast(`Đã ${action === 'CHILD_LOCK_ON' ? 'BẬT Khóa Trẻ Em' : 'TẮT Khóa Trẻ Em'}!`, 'success');
  } catch (err) {
    e.target.checked = !e.target.checked;
    showToast(err.message, 'error');
  }
}

// ==================== DEVICE MANAGEMENT ====================
async function handleAddDevice(e) {
  e.preventDefault();
  const device_code = document.getElementById('addDeviceCode').value.trim();
  const name = document.getElementById('addDeviceName').value.trim();

  try {
    await apiCall('/devices/', 'POST', { device_code, name });
    closeModal(addDeviceModal);
    showToast('Đã thêm máy bơm mới thành công!', 'success');
    await loadDevices();
  } catch (err) {
    showToast(err.message, 'error');
  }
}

async function handleShareDevice(e) {
  e.preventDefault();
  if (!currentDevice) return;
  const target_email = document.getElementById('shareTargetEmail').value.trim();
  const permission = document.getElementById('sharePermissionSelect').value;

  try {
    await apiCall(`/devices/${currentDevice.id}/share`, 'POST', { target_email, permission });
    closeModal(shareDeviceModal);
    showToast(`Đã chia sẻ quyền cho ${target_email} thành công!`, 'success');
    document.getElementById('shareTargetEmail').value = '';
  } catch (err) {
    showToast(err.message, 'error');
  }
}

// ==================== MODAL & TOAST HELPERS ====================
function openModal(modalEl) {
  modalEl.classList.add('active');
}
function closeModal(modalEl) {
  modalEl.classList.remove('active');
}

let toastTimer = null;
function showToast(msg, type = 'success') {
  toastMessage.textContent = msg;
  appToast.className = `toast active ${type}`;
  
  const icon = document.getElementById('toastIcon');
  icon.className = type === 'success' ? 'fa-solid fa-circle-check toast-icon' : 'fa-solid fa-circle-exclamation toast-icon';

  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => {
    appToast.classList.remove('active');
  }, 3500);
}

// ==================== TANK CONFIGURATION ====================
function handleOpenTankConfig() {
  if (!currentDevice) {
    showToast('Chưa chọn máy bơm nào!', 'error');
    return;
  }
  cfgTankHeight.value = currentDevice.tank_height_cm || 120;
  cfgTankOffset.value = currentDevice.sensor_offset_cm || 10;
  cfgMinWaterPct.value = currentDevice.min_water_percent || 20;
  cfgMaxWaterPct.value = currentDevice.max_water_percent || 90;
  openModal(tankConfigModal);
}

async function handleSaveTankConfig(e) {
  e.preventDefault();
  if (!currentDevice) return;

  const tank_height = parseFloat(cfgTankHeight.value);
  const offset = parseFloat(cfgTankOffset.value);
  const min_pct = parseInt(cfgMinWaterPct.value);
  const max_pct = parseInt(cfgMaxWaterPct.value);

  if (min_pct >= max_pct) {
    showToast('Ngưỡng tự bật (Min) phải nhỏ hơn ngưỡng tự ngắt (Max)!', 'error');
    return;
  }

  try {
    showToast('Đang lưu và gửi thông số xuống ESP32...', 'success');
    const res = await apiCall(`/pumps/${currentDevice.id}/config`, 'POST', {
      tank_height,
      offset,
      min_pct,
      max_pct
    });
    currentDevice.tank_height_cm = tank_height;
    currentDevice.sensor_offset_cm = offset;
    currentDevice.min_water_percent = min_pct;
    currentDevice.max_water_percent = max_pct;
    closeModal(tankConfigModal);
    showToast(res.message || 'Đã lưu cấu hình téc nước thành công!', 'success');
    fetchDeviceLogs();
  } catch (err) {
    showToast(err.message, 'error');
  }
}

// ==================== VIEW SWITCHING & NAVIGATION ====================
function switchMainView(viewName) {
  const navHome = document.getElementById('navHome');
  const navAdmin = document.getElementById('navAdmin');
  const navLogs = document.getElementById('navLogs');
  const viewAdminPortal = document.getElementById('viewAdminPortal');
  const currentViewTitle = document.getElementById('currentViewTitle');
  const appSidebar = document.getElementById('appSidebar');
  const sidebarBackdrop = document.getElementById('sidebarBackdrop');

  // Đóng drawer trên màn hình di động khi click menu
  if (appSidebar) appSidebar.classList.remove('active');
  if (sidebarBackdrop) sidebarBackdrop.classList.remove('active');

  if (viewName === 'admin') {
    if (!currentUser || currentUser.role !== 'ADMIN') {
      showToast('Bạn không có quyền truy cập trang Quản Trị!', 'error');
      return;
    }
    mainDashboard.style.display = 'none';
    if (viewAdminPortal) viewAdminPortal.style.display = 'block';
    if (navHome) navHome.classList.remove('active');
    if (navAdmin) navAdmin.classList.add('active');
    if (navLogs) navLogs.classList.remove('active');
    if (currentViewTitle) currentViewTitle.textContent = 'Trang Quản Trị Hệ Thống (Admin Portal)';
    switchAdminTab('users');
    loadAdminUsers();
    loadAdminFirmwares();
  } else if (viewName === 'logs') {
    mainDashboard.style.display = 'block';
    if (viewAdminPortal) viewAdminPortal.style.display = 'none';
    if (navHome) navHome.classList.remove('active');
    if (navAdmin) navAdmin.classList.remove('active');
    if (navLogs) navLogs.classList.add('active');
    if (currentViewTitle) currentViewTitle.textContent = 'Nhật Ký Vận Hành';
    const sectionLogs = document.getElementById('sectionLogs');
    if (sectionLogs) sectionLogs.scrollIntoView({ behavior: 'smooth' });
  } else {
    // Trang Chủ Dashboard
    mainDashboard.style.display = 'block';
    if (viewAdminPortal) viewAdminPortal.style.display = 'none';
    if (navHome) navHome.classList.add('active');
    if (navAdmin) navAdmin.classList.remove('active');
    if (navLogs) navLogs.classList.remove('active');
    if (currentViewTitle) currentViewTitle.textContent = 'Trang Chủ (Giám Sát Bơm)';
  }
}

// ==================== ADMIN PORTAL & USER CRUD ====================
let adminUsersList = [];

function handleOpenAdminPortal() {
  switchMainView('admin');
}

function switchAdminTab(tab) {
  if (tab === 'users') {
    tabAdminUsers.classList.add('active');
    tabAdminOta.classList.remove('active');
    adminUsersSection.style.display = 'block';
    adminOtaSection.style.display = 'none';
  } else {
    tabAdminOta.classList.add('active');
    tabAdminUsers.classList.remove('active');
    adminUsersSection.style.display = 'none';
    adminOtaSection.style.display = 'block';
  }
}

async function loadAdminUsers() {
  try {
    adminUsersList = await apiCall('/admin/users');
    renderAdminUsersTable();
  } catch (err) {
    showToast(err.message, 'error');
  }
}

function renderAdminUsersTable() {
  if (!adminUsersList || adminUsersList.length === 0) {
    adminUsersTableBody.innerHTML = `<tr><td colspan="6" class="text-center py-4 text-muted">Chưa có người dùng nào.</td></tr>`;
    return;
  }

  adminUsersTableBody.innerHTML = adminUsersList.map(u => {
    const isMe = (currentUser && currentUser.id === u.id);
    const createdStr = new Date(u.created_at).toLocaleDateString('vi-VN');
    const roleBadge = u.role === 'ADMIN' 
      ? `<span class="badge" style="background:rgba(245,158,11,0.2); color:#fbbf24; border-color:#f59e0b;">ADMIN</span>`
      : `<span class="badge">USER</span>`;

    return `
      <tr>
        <td><b>${u.full_name}</b> ${isMe ? '<small class="text-muted">(Bạn)</small>' : ''}</td>
        <td>${u.email}</td>
        <td>${roleBadge}</td>
        <td>${u.owned_devices_count} máy bơm</td>
        <td>${createdStr}</td>
        <td>
          <button class="btn btn-outline btn-sm" onclick="handleEditUser('${u.id}')" title="Sửa">
            <i class="fa-solid fa-pen-to-square"></i>
          </button>
          ${!isMe ? `
          <button class="btn btn-outline btn-sm" style="color:var(--danger); border-color:var(--danger);" onclick="handleDeleteUser('${u.id}', '${u.email}')" title="Xóa">
            <i class="fa-solid fa-trash"></i>
          </button>` : ''}
        </td>
      </tr>
    `;
  }).join('');
}

function handleOpenAddUserModal() {
  adminUserModalTitle.innerHTML = '<i class="fa-solid fa-user-plus"></i> Thêm Người Dùng Mới';
  adminUserId.value = '';
  adminUserFullName.value = '';
  adminUserEmail.value = '';
  adminUserRole.value = 'USER';
  adminUserPassword.value = '';
  adminUserPassword.required = true;
  adminUserPassLabel.textContent = 'Mật Khẩu *';
  openModal(adminUserModal);
}

window.handleEditUser = function(userId) {
  const user = adminUsersList.find(u => u.id === userId);
  if (!user) return;
  adminUserModalTitle.innerHTML = '<i class="fa-solid fa-user-pen"></i> Chỉnh Sửa Người Dùng';
  adminUserId.value = user.id;
  adminUserFullName.value = user.full_name;
  adminUserEmail.value = user.email;
  adminUserRole.value = user.role;
  adminUserPassword.value = '';
  adminUserPassword.required = false;
  adminUserPassLabel.textContent = 'Mật Khẩu Mới (Để trống nếu giữ nguyên)';
  openModal(adminUserModal);
};

window.handleDeleteUser = async function(userId, email) {
  if (!confirm(`Bạn có chắc chắn muốn xóa vĩnh viễn tài khoản "${email}" không?`)) {
    return;
  }
  try {
    const res = await apiCall(`/admin/users/${userId}`, 'DELETE');
    showToast(res.message || 'Đã xóa người dùng thành công!', 'success');
    await loadAdminUsers();
  } catch (err) {
    showToast(err.message, 'error');
  }
};

async function handleSaveAdminUser(e) {
  e.preventDefault();
  const id = adminUserId.value;
  const full_name = adminUserFullName.value.trim();
  const email = adminUserEmail.value.trim();
  const role = adminUserRole.value;
  const password = adminUserPassword.value;

  try {
    if (!id) {
      // Thêm mới
      await apiCall('/admin/users', 'POST', { full_name, email, role, password });
      showToast('Đã tạo tài khoản mới thành công!', 'success');
    } else {
      // Cập nhật
      const body = { full_name, role };
      if (password && password.trim().length > 0) {
        body.password = password.trim();
      }
      await apiCall(`/admin/users/${id}`, 'PUT', body);
      showToast('Đã cập nhật thông tin người dùng thành công!', 'success');
    }
    closeModal(adminUserModal);
    await loadAdminUsers();
  } catch (err) {
    showToast(err.message, 'error');
  }
}

// ==================== DUAL-NODE OTA FIRMWARE ====================
async function loadAdminFirmwares() {
  try {
    const files = await apiCall('/admin/ota/firmwares');
    if (!files || files.length === 0) {
      adminFirmwareTableBody.innerHTML = `<tr><td colspan="3" class="text-center py-4 text-muted">Chưa có bản firmware nào.</td></tr>`;
      return;
    }
    adminFirmwareTableBody.innerHTML = files.map(f => {
      const sizeKB = (f.size / 1024).toFixed(1);
      return `
        <tr>
          <td><i class="fa-solid fa-file-lines text-muted"></i> <b>${f.filename}</b></td>
          <td>${sizeKB} KB</td>
          <td>
            <a href="${f.download_url}" download class="btn btn-secondary btn-sm" title="Tải về máy">
              <i class="fa-solid fa-download"></i>
            </a>
          </td>
        </tr>
      `;
    }).join('');
  } catch (err) {
    console.error('Lỗi tải danh sách firmware:', err);
  }
}

async function handleTriggerOta(e) {
  e.preventDefault();
  const target = otaTargetSelect.value;
  const version = otaVersionInput.value.trim();
  const file = otaBinFileInput.files[0];

  if (!file) {
    showToast('Vui lòng chọn file firmware (.bin)!', 'error');
    return;
  }

  const otaProgressBar = document.getElementById('otaProgressBar');
  const otaPercentText = document.getElementById('otaPercentText');
  const otaBytesInfo = document.getElementById('otaBytesInfo');

  otaStatusBox.style.display = 'flex';
  if (otaSpinner) otaSpinner.style.display = 'inline-block';
  if (btnSubmitOta) btnSubmitOta.disabled = true;
  if (otaProgressBar) {
    otaProgressBar.style.width = '0%';
    otaProgressBar.style.background = 'linear-gradient(90deg, #00f2fe, #4facfe)';
  }
  if (otaPercentText) otaPercentText.textContent = '0%';
  if (otaBytesInfo) otaBytesInfo.textContent = `Chuẩn bị tải lên VPS: ${file.name}`;
  otaStatusText.textContent = `Đang tải file lên VPS...`;

  try {
    // 1. Upload file binary
    const formData = new FormData();
    formData.append('file', file);

    const uploadHeaders = {};
    if (token) uploadHeaders['Authorization'] = `Bearer ${token}`;

    const uploadRes = await fetch(`${API_BASE}/admin/ota/upload`, {
      method: 'POST',
      headers: uploadHeaders,
      body: formData
    });

    const uploadData = await uploadRes.json();
    if (!uploadRes.ok) {
      throw new Error(uploadData.detail || 'Lỗi khi upload file firmware lên VPS!');
    }

    // 2. Kích hoạt lệnh OTA qua MQTT
    const targetHost = window.location.hostname || '180.93.113.40';
    const targetPort = window.location.port ? `:${window.location.port}` : '';
    const fullDownloadUrl = `http://${targetHost}${targetPort}${uploadData.download_url}`;

    otaStatusText.textContent = `Đã upload! Đang kích hoạt nạp OTA xuống ${target === 'esp32s3_cabinet' ? 'Tủ Điện S3' : 'Node Bể Nước'}...`;

    const triggerRes = await apiCall('/admin/ota/trigger', 'POST', {
      target: target,
      version: version,
      url: fullDownloadUrl,
      filename: uploadData.filename,
      size: uploadData.size,
      md5: uploadData.md5
    });

    await loadAdminFirmwares();

    // Hiển thị trạng thái đang nạp & thanh tiến trình
    otaStatusText.innerHTML = `<span>Đang nạp firmware xuống thiết bị...</span>`;
    if (otaBytesInfo) otaBytesInfo.textContent = `0 KB / ${(uploadData.size / 1024).toFixed(0)} KB`;

    // Ghi nhớ phiên bản ban đầu để so sánh chính xác sau khi reboot
    const initialVer = target === 'esp32_tank'
      ? (currentDevice?.tank_firmware_version || '1.0.0')
      : (currentDevice?.firmware_version || '1.0.0');

    let pollCount = 0;
    let hasReachedReboot = false;
    let lastProgressPct = -1;
    let lastProgressBytes = -1;
    let frozenSeconds = 0;

    const pollInterval = setInterval(async () => {
      pollCount++;
      let progRes = null;
      try {
        progRes = await apiCall('/admin/ota/progress');
        if (progRes && progRes.target === target) {
          const pct = Math.max(0, Math.min(100, progRes.percent || 0));
          const currentBytes = progRes.bytes || 0;
          if (pct > lastProgressPct || currentBytes > lastProgressBytes) {
            lastProgressPct = pct;
            lastProgressBytes = currentBytes;
            frozenSeconds = 0; // Reset watchdog khi tiến độ hoặc số bytes đang tăng tốt
          } else {
            frozenSeconds++;
          }

          if (otaProgressBar) otaProgressBar.style.width = `${pct}%`;
          if (otaPercentText) otaPercentText.textContent = `${pct}%`;

          if (progRes.bytes && progRes.total) {
            const kbRead = (progRes.bytes / 1024).toFixed(0);
            const kbTotal = (progRes.total / 1024).toFixed(0);
            if (otaBytesInfo) otaBytesInfo.textContent = `${kbRead} KB / ${kbTotal} KB`;
          }

          // 1. Nếu nhận được báo cáo thất bại hoặc Rollback thực sự từ thiết bị
          // Bỏ qua nếu là cache cũ sót lại từ phiên trước khi mới bấm nút (pollCount <= 4 và pct >= 100)
          const isStalePreviousFail = (pollCount <= 4 && pct >= 100 && progRes.status === 'failed');
          if (!isStalePreviousFail && (progRes.status === 'failed' || (progRes.error && progRes.error.includes('ROLLBACK')))) {
            clearInterval(pollInterval);
            if (otaSpinner) otaSpinner.style.display = 'none';
            if (btnSubmitOta) btnSubmitOta.disabled = false;
            if (otaProgressBar) otaProgressBar.style.background = 'linear-gradient(90deg, #ef4444, #b91c1c)';
            const errMsg = progRes.message || (progRes.error === 'ROLLBACK_APP_CRASHED' 
              ? 'Bản firmware mới bị crash khi khởi động, đã tự động Rollback về bản cũ!'
              : progRes.error || 'Nạp thất bại');
            otaStatusText.innerHTML = `<i class="fa-solid fa-triangle-exclamation" style="color:var(--danger)"></i> <b style="color:var(--danger)">CẬP NHẬT THẤT BẠI: ${errMsg}</b>`;
            showToast(`⚠️ CẬP NHẬT THẤT BẠI: ${errMsg}`, 'error');
            await fetchDeviceData(false);
            updateOtaTargetVersionDisplay();
            return;
          }

          // 2. Khi thiết bị đã ghi flash xong 100% và đang reboot
          if (pct >= 100 || progRes.status === 'rebooting' || progRes.status === 'success') {
            hasReachedReboot = true;
            if (otaProgressBar) {
              otaProgressBar.style.width = '100%';
              otaProgressBar.style.background = 'linear-gradient(90deg, #0284c7, #38bdf8)';
            }
            if (otaPercentText) otaPercentText.textContent = '100%';
            if (otaSpinner) otaSpinner.style.display = 'inline-block';
            otaStatusText.innerHTML = `<i class="fa-solid fa-arrows-rotate fa-spin" style="color:#38bdf8"></i> <b style="color:#38bdf8;">Đã nạp xong 100%! Đang chờ thiết bị khởi động lại và xác nhận an toàn...</b>`;
          }
        }
      } catch (e) {
        // bỏ qua lỗi polling mạng tạm thời
      }

      // 3. Kiểm tra thông tin thiết bị sau khi reboot
      await fetchDeviceData(false);
      updateOtaTargetVersionDisplay();

      const currentVer = target === 'esp32_tank'
        ? currentDevice?.tank_firmware_version
        : currentDevice?.firmware_version;

      // ĐIỀU KIỆN THÀNH CÔNG:
      // Tuyệt đối không xác nhận thành công nếu backend hoặc thiết bị đang báo lỗi (failed)
      if (progRes && (progRes.status === 'failed' || progRes.error)) {
        return; // Để nhánh failed xử lý
      }

      const isVerMatched = (currentVer === version || currentVer === `v${version}` || (initialVer && currentVer && currentVer !== initialVer));
      const isStatusSuccess = (progRes && progRes.status === 'success');
      const isOtaSuccess = (isStatusSuccess && pct >= 100) || (hasReachedReboot && (isVerMatched || isStatusSuccess));

      if (isOtaSuccess) {
        // Kiểm tra lại lần cuối xem có cờ lỗi không
        const finalProg = await apiCall('/admin/ota/progress').catch(() => null);
        if (finalProg && (finalProg.status === 'failed' || finalProg.error)) {
          return;
        }
        clearInterval(pollInterval);
        if (otaProgressBar) {
          otaProgressBar.style.width = '100%';
          otaProgressBar.style.background = 'linear-gradient(90deg, #10b981, #059669)';
        }
        if (otaPercentText) otaPercentText.textContent = '100%';
        if (otaSpinner) otaSpinner.style.display = 'none';
        if (btnSubmitOta) btnSubmitOta.disabled = false;
        otaStatusText.innerHTML = `<i class="fa-solid fa-circle-check" style="color:#10b981; font-size:1.2rem;"></i> <b style="color:#10b981;">NÂNG CẤP THÀNH CÔNG! Thiết bị đang chạy v${version}</b>`;
        showToast(`🎉 NÂNG CẤP THÀNH CÔNG! Thiết bị đã cập nhật lên phiên bản v${version}!`, 'success');
        updateOtaTargetVersionDisplay();
        return;
      }

      // 4. Hết thời gian chờ THỰC SỰ (Chỉ timeout khi tiến độ bị đóng băng hoàn toàn quá 60 giây)
      if (frozenSeconds >= 60) {
        clearInterval(pollInterval);
        if (btnSubmitOta) btnSubmitOta.disabled = false;
        if (otaSpinner) otaSpinner.style.display = 'none';
        if (currentVer !== version && currentVer !== `v${version}`) {
          if (otaProgressBar) otaProgressBar.style.background = 'linear-gradient(90deg, #ef4444, #b91c1c)';
          otaStatusText.innerHTML = `<i class="fa-solid fa-triangle-exclamation" style="color:var(--danger)"></i> <b style="color:var(--danger);">HẾT THỜI GIAN CHỜ: Thiết bị không phản hồi bản v${version} (hiện tại: v${currentVer || '1.0.0'}).</b>`;
          showToast(`⚠️ Quá thời gian nạp OTA! Thiết bị không phản hồi.`, 'error');
        }
      }
    }, 1000);
  } catch (err) {
    if (otaSpinner) otaSpinner.style.display = 'none';
    if (btnSubmitOta) btnSubmitOta.disabled = false;
    otaStatusText.innerHTML = `<i class="fa-solid fa-circle-xmark" style="color:var(--danger); font-size:1.1rem; margin-right:6px;"></i> Lỗi: ${err.message}`;
    showToast(err.message, 'error');
  }
}

function updateOtaTargetVersionDisplay() {
  const otaCurrentTargetVer = document.getElementById('otaCurrentTargetVer');
  const otaTargetSelect = document.getElementById('otaTargetSelect');
  if (!otaCurrentTargetVer || !otaTargetSelect) return;

  const target = otaTargetSelect.value;
  if (!currentDevice) {
    otaCurrentTargetVer.textContent = '---';
    return;
  }
  if (target === 'esp32_tank') {
    otaCurrentTargetVer.textContent = `v${currentDevice.tank_firmware_version || '1.0.0'}`;
  } else {
    otaCurrentTargetVer.textContent = `v${currentDevice.firmware_version || '1.0.0'}`;
  }
}

const otaTargetSelectEl = document.getElementById('otaTargetSelect');
if (otaTargetSelectEl) {
  otaTargetSelectEl.addEventListener('change', updateOtaTargetVersionDisplay);
}
