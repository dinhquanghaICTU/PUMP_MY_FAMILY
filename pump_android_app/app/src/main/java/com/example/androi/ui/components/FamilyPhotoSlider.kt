package com.example.androi.ui.components

import androidx.compose.animation.core.tween
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.collectIsDraggedAsState
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import com.example.androi.R
import com.example.androi.ui.theme.BrandOrange
import com.example.androi.ui.theme.TextSecondary
import kotlinx.coroutines.delay

/**
 * Danh sách 3 ảnh gia đình từ res/drawable
 */
val familyPhotoResources = listOf(
    R.drawable.family_photo_1,
    R.drawable.family_photo_2,
    R.drawable.family_photo_3
)

/**
 * Slide ảnh gia đình vuốt ngang (Auto-sliding Carousel):
 * - Hiển thị ảnh trọn vẹn, bỏ chữ tiêu đề đè lên
 * - Tự động chuyển ảnh sau mỗi 3.5 giây nếu người dùng không vuốt
 * - Người dùng vẫn có thể vuốt tay qua lại tự do
 */
@Composable
fun FamilyPhotoSlider(
    modifier: Modifier = Modifier,
    photos: List<Int> = familyPhotoResources,
    onPhotoClick: (Int) -> Unit = {}
) {
    val pagerState = rememberPagerState(pageCount = { photos.size })
    val isDragged by pagerState.interactionSource.collectIsDraggedAsState()

    // Tự động chuyển slide sau mỗi 3.5s (tạm dừng khi người dùng đang giữ/vuốt tay)
    LaunchedEffect(isDragged) {
        if (!isDragged) {
            while (true) {
                delay(3500)
                if (!isDragged && photos.isNotEmpty()) {
                    val nextPage = (pagerState.currentPage + 1) % photos.size
                    pagerState.animateScrollToPage(
                        page = nextPage,
                        animationSpec = tween(durationMillis = 750)
                    )
                }
            }
        }
    }

    Column(modifier = modifier.fillMaxWidth()) {
        HorizontalPager(
            state = pagerState,
            contentPadding = PaddingValues(horizontal = 24.dp),
            pageSpacing = 14.dp,
            modifier = Modifier
                .fillMaxWidth()
                .height(230.dp)
        ) { page ->
            val photoRes = photos[page]
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .shadow(elevation = 6.dp, shape = RoundedCornerShape(26.dp), spotColor = BrandOrange.copy(alpha = 0.2f))
                    .clip(RoundedCornerShape(26.dp))
                    .clickable { onPhotoClick(photoRes) }
            ) {
                Image(
                    painter = painterResource(id = photoRes),
                    contentDescription = "Ảnh Gia Đình",
                    contentScale = ContentScale.Crop,
                    modifier = Modifier.fillMaxSize()
                )
            }
        }

        Spacer(modifier = Modifier.height(14.dp))

        // Chấm tròn chỉ báo slide (Dots Indicator)
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp),
            horizontalArrangement = Arrangement.Center,
            verticalAlignment = Alignment.CenterVertically
        ) {
            repeat(photos.size) { index ->
                val isSelected = pagerState.currentPage == index
                Box(
                    modifier = Modifier
                        .padding(horizontal = 4.dp)
                        .height(6.dp)
                        .width(if (isSelected) 24.dp else 6.dp)
                        .clip(CircleShape)
                        .background(
                            if (isSelected) BrandOrange else TextSecondary.copy(alpha = 0.3f)
                        )
                )
            }
        }
    }
}
