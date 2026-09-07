package com.example.androi.ui.components

import android.graphics.BitmapFactory
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.CameraAlt
import androidx.compose.material.icons.rounded.Notifications
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.androi.R
import com.example.androi.ui.theme.BrandOrange
import com.example.androi.ui.theme.CardBackground
import com.example.androi.ui.theme.TextPrimary
import com.example.androi.ui.theme.TextSecondary
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

/**
 * Phần đầu trang (Header) với:
 * - Avatar cá nhân có thể bấm vào để chọn/cập nhật ảnh từ thư viện máy (PickVisualMedia)
 * - Tự động lưu ảnh đại diện vào bộ nhớ trong để không bị mất khi đóng app
 * - Lời chào cá nhân hóa
 * - Chuông thông báo
 */
@Composable
fun HeaderGreeting(
    userName: String = "Hà",
    greeting: String = "Chào gia đình nhỏ",
    onNotificationClick: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()
    var avatarBitmap by remember { mutableStateOf<ImageBitmap?>(null) }

    // Đọc ảnh avatar đã lưu trong bộ nhớ máy khi khởi chạy
    LaunchedEffect(Unit) {
        val avatarFile = File(context.filesDir, "user_profile_avatar.jpg")
        if (avatarFile.exists()) {
            withContext(Dispatchers.IO) {
                try {
                    val bitmap = BitmapFactory.decodeFile(avatarFile.absolutePath)
                    if (bitmap != null) {
                        avatarBitmap = bitmap.asImageBitmap()
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }
        }
    }

    // Trình chọn ảnh từ thư viện máy (Android Photo Picker)
    val photoPickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.PickVisualMedia()
    ) { uri: Uri? ->
        if (uri != null) {
            coroutineScope.launch(Dispatchers.IO) {
                try {
                    context.contentResolver.openInputStream(uri)?.use { inputStream ->
                        val avatarFile = File(context.filesDir, "user_profile_avatar.jpg")
                        avatarFile.outputStream().use { outputStream ->
                            inputStream.copyTo(outputStream)
                        }
                        val bitmap = BitmapFactory.decodeFile(avatarFile.absolutePath)
                        if (bitmap != null) {
                            withContext(Dispatchers.Main) {
                                avatarBitmap = bitmap.asImageBitmap()
                            }
                        }
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }
        }
    }

    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 24.dp, vertical = 16.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            // Avatar tròn bấm vào để cập nhật ảnh
            Box(
                modifier = Modifier
                    .size(52.dp)
                    .clickable {
                        photoPickerLauncher.launch(
                            PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly)
                        )
                    }
            ) {
                // Khung ảnh avatar
                Box(
                    modifier = Modifier
                        .size(50.dp)
                        .clip(CircleShape)
                        .border(2.dp, BrandOrange.copy(alpha = 0.5f), CircleShape)
                        .background(CardBackground),
                    contentAlignment = Alignment.Center
                ) {
                    if (avatarBitmap != null) {
                        Image(
                            bitmap = avatarBitmap!!,
                            contentDescription = "Avatar",
                            contentScale = ContentScale.Crop,
                            modifier = Modifier.fillMaxSize()
                        )
                    } else {
                        // Mặc định hiển thị ảnh chân dung của bạn nếu chưa tải ảnh riêng
                        Image(
                            painter = painterResource(id = R.drawable.family_photo_3),
                            contentDescription = "Avatar",
                            contentScale = ContentScale.Crop,
                            modifier = Modifier.fillMaxSize()
                        )
                    }
                }

                // Huy hiệu icon Camera nhỏ góc dưới báo hiệu có thể đổi ảnh
                Box(
                    modifier = Modifier
                        .size(19.dp)
                        .clip(CircleShape)
                        .background(BrandOrange)
                        .border(1.5.dp, Color.White, CircleShape)
                        .align(Alignment.BottomEnd),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = Icons.Rounded.CameraAlt,
                        contentDescription = "Đổi ảnh đại diện",
                        tint = Color.White,
                        modifier = Modifier.size(11.dp)
                    )
                }
            }

            Column {
                Text(
                    text = "Xin chào, $userName",
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    color = TextPrimary
                )
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    text = greeting,
                    fontSize = 13.sp,
                    color = TextSecondary
                )
            }
        }

        // Nút chuông thông báo hình tròn nền trắng
        Box(
            modifier = Modifier
                .size(44.dp)
                .clip(CircleShape)
                .background(CardBackground)
                .clickable { onNotificationClick() },
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = Icons.Rounded.Notifications,
                contentDescription = "Notifications",
                tint = TextPrimary,
                modifier = Modifier.size(22.dp)
            )
        }
    }
}
