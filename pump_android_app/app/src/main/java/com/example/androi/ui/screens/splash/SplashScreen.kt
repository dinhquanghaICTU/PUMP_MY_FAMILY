package com.example.androi.ui.screens.splash

import androidx.compose.animation.core.*
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Code
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.androi.R
import com.example.androi.ui.theme.BrandOrange
import com.example.androi.ui.theme.ScreenBackground
import com.example.androi.ui.theme.SurfaceVariant
import com.example.androi.ui.theme.TextPrimary
import com.example.androi.ui.theme.TextSecondary
import kotlinx.coroutines.delay

/**
 * Màn hình Chào khởi động (Splash Screen):
 * - Logo máy bơm thông minh 3D hiện đại
 * - Dòng chữ "Xây dựng và phát triển bởi QUANG HÀ ICTU"
 * - Hiệu ứng chuyển động mượt mà trước khi vào màn hình chính
 */
@Composable
fun SplashScreen(
    onSplashComplete: () -> Unit = {}
) {
    val scale = remember { Animatable(0.75f) }
    val alpha = remember { Animatable(0f) }

    LaunchedEffect(Unit) {
        // Hiệu ứng phóng to nhẹ & mờ dần hiện rõ
        scale.animateTo(
            targetValue = 1f,
            animationSpec = tween(
                durationMillis = 800,
                easing = FastOutSlowInEasing
            )
        )
    }

    LaunchedEffect(Unit) {
        alpha.animateTo(
            targetValue = 1f,
            animationSpec = tween(durationMillis = 700)
        )
        // Hiển thị 2.3 giây rồi chuyển tiếp vào app chính
        delay(1500)
        onSplashComplete()
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(ScreenBackground)
            .padding(32.dp),
        contentAlignment = Alignment.Center
    ) {
        // Khối trung tâm: Logo + Tên App
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            modifier = Modifier
                .scale(scale.value)
                .alpha(alpha.value)
        ) {
            // Khung chứa Logo Máy Bơm
            Box(
                modifier = Modifier
                    .size(160.dp)
                    .shadow(
                        elevation = 16.dp,
                        shape = RoundedCornerShape(36.dp),
                        spotColor = BrandOrange.copy(alpha = 0.35f)
                    )
                    .clip(RoundedCornerShape(36.dp))
                    .background(Color.White),
                contentAlignment = Alignment.Center
            ) {
                Image(
                    painter = painterResource(id = R.drawable.app_logo_pump),
                    contentDescription = "Logo Máy Bơm Thông Minh",
                    contentScale = ContentScale.Crop,
                    modifier = Modifier.fillMaxSize()
                )
            }

            Spacer(modifier = Modifier.height(28.dp))

            // Tên ứng dụng
            Text(
                text = "PUMP MY FAMILY",
                fontSize = 24.sp,
                fontWeight = FontWeight.Black,
                color = TextPrimary,
                letterSpacing = 1.sp
            )

            Spacer(modifier = Modifier.height(6.dp))

            Text(
                text = "Hệ Thống Bơm & Bể Nước Thông Minh",
                fontSize = 14.sp,
                fontWeight = FontWeight.Medium,
                color = TextSecondary
            )
        }

        // Khối chân trang (Footer): Tác giả & Trường ICTU
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .alpha(alpha.value)
                .padding(bottom = 20.dp)
        ) {
            Text(
                text = "Xây dựng và phát triển bởi",
                fontSize = 13.sp,
                color = TextSecondary,
                fontWeight = FontWeight.Normal
            )

            Spacer(modifier = Modifier.height(5.dp))

            // Tên tác giả QUANG HÀ ICTU nổi bật màu cam thương hiệu
            Text(
                text = "QUANG HÀ ICTU",
                fontSize = 17.sp,
                fontWeight = FontWeight.Bold,
                color = BrandOrange,
                letterSpacing = 0.8.sp
            )

            Spacer(modifier = Modifier.height(8.dp))

            // Badge ICTU
            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(12.dp))
                    .background(SurfaceVariant)
                    .padding(horizontal = 14.dp, vertical = 6.dp)
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        imageVector = Icons.Rounded.Code,
                        contentDescription = null,
                        tint = BrandOrange,
                        modifier = Modifier.size(15.dp)
                    )
                    Spacer(modifier = Modifier.width(6.dp))
                    Text(
                        text = "Đại học CNTT & TT Thái Nguyên",
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Medium,
                        color = TextPrimary
                    )
                }
            }
        }
    }
}
