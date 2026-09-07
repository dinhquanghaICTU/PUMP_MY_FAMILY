package com.example.androi.data.model

/**
 * Trạng thái của thiết bị máy bơm & bể nước (để sẵn cho bạn tự bind ViewModel & API)
 */
data class DeviceUiState(
    val deviceCode: String = "PUMP_ESP32S3_01",
    val name: String = "Máy Bơm Gia Đình",
    val isOnline: Boolean = true,
    val isPumpRunning: Boolean = false,
    val isAutoMode: Boolean = true,
    val isChildLock: Boolean = false,
    val waterPercent: Float = 89.6f,
    val distanceCm: Float = 37.5f,
    val batteryVoltage: Float = 4.15f,
    val pumpRuntimeSec: Int = 0,
    val firmwareVersion: String = "1.0.4",
    val tankFirmwareVersion: String = "1.0.4"
)

/**
 * Thẻ thiết bị hiển thị trong danh sách / lưới 2x2
 */
data class DeviceCardItem(
    val id: String,
    val title: String,
    val subtitle: String,
    val iconType: DeviceIconType,
    val isRunning: Boolean,
    val statusText: String,
    val isSwitchEnabled: Boolean = true
)

enum class DeviceIconType {
    PUMP,
    WATER_TANK,
    CHILD_LOCK,
    SOLAR_BATTERY,
    LIGHT,
    AC
}

/**
 * Bộ lọc phòng / khu vực
 */
data class RoomCategory(
    val id: String,
    val name: String,
    val isSelected: Boolean = false
)
