FROM nginx:alpine

# Xóa cấu hình mặc định
RUN rm /etc/nginx/conf.d/default.conf

# Copy cấu hình Nginx riêng
COPY nginx.conf /etc/nginx/conf.d/default.conf

# Copy toàn bộ file web vào thư mục phục vụ của Nginx
COPY . /usr/share/nginx/html

EXPOSE 80

CMD ["nginx", "-g", "daemon off;"]
