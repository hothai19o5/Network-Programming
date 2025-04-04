#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <malloc.h>

void append(char **dst, const char *src)
{
    // Thiếu bước kiểm tra src rỗng thì thoát luôn
    int old_len = *dst == NULL ? 0 : strlen(*dst);
    int new_len = old_len + strlen(src);
    *dst = (char *)realloc(*dst, new_len + 1);
    memset(*dst + old_len, 0, new_len - old_len + 1);
    strcat(*dst, src);
}

/*
    struct dirent là một cấu trúc dữ liệu (struct) dùng để lưu trữ thông tin về một mục (file hoặc thư mục) khi duyệt thư mục bằng các hàm như scandir() hoặc readdir()

    struct dirent {
        ino_t          d_ino;       // inode number
        off_t          d_off;       // offset to the next dirent
        unsigned short d_reclen;    // length of this record
        unsigned char  d_type;      // type of file; not supported by all file system types
        char           d_name[256]; // filename
    };

    Hàm scandir trả về số lượng file và thư mục trong thư mục được truyền vào.

    int scandir(const char *dirp, struct dirent ***namelist, int (*filter)(const struct dirent *),
        int (*compar)(const struct dirent **, const struct dirent **));

        dirp: Đường dẫn tới thư mục cần quét.
        namelist: Con trỏ tới một mảng con trỏ struct dirent*, chứa danh sách các tệp tìm thấy.
        filter: Hàm con trỏ dùng để lọc kết quả (NULL nếu không muốn lọc).
        compar: Hàm con trỏ dùng để sắp xếp danh sách (NULL nếu không muốn sắp xếp).

*/

// Hàm con trỏ dùng để sắp xếp danh sách (NULL nếu không muốn sắp xếp).
int compare(const struct dirent **a, const struct dirent **b)
{
    return strcmp((*a)->d_name, (*b)->d_name);
}

/*
    argc (argument count) là số lượng tham số truyền vào chương trình từ dòng lệnh, bao gồm cả tên chương trình.
    argv (argument vector) là mảng các chuỗi chứa các tham số truyền vào.   
*/
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Sai tham so");
        return 1;
    }

    struct dirent **namelist;
    int n = scandir(argv[1], &namelist, NULL, compare);
    if (n < 0)
    {
        perror("scandir");
        return 1;
    }
    char *s = NULL;
    append(&s, "<html><body>");
    for (int i = 0; i < n; i++)
    {
        // d_type: kiểu của file hoặc thư mục
        // DT_REG: file thường
        // DT_DIR: thư mục
        if (namelist[i]->d_type == DT_REG)
        {
            append(&s, "<i>");
            append(&s, namelist[i]->d_name);
            append(&s, "</i><br/>");
        }
        else if (namelist[i]->d_type == DT_DIR)
        {
            append(&s, "<b>");
            append(&s, namelist[i]->d_name);
            append(&s, "</b><br/>");
        }
        free(namelist[i]);
    }
    append(&s, "</body></html>");
    free(namelist);
    printf("%s", s);
    FILE *f = fopen("my_scan.html", "wb");
    fwrite(s, 1, strlen(s), f);
    fclose(f);
    free(s);
    return 0;
}