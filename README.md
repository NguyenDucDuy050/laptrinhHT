# Bao cao giua ky: Implement ls(1)

- Ho ten: Nguyen Duc Duy
- MSSV: 24IT050
- Repository: https://github.com/NguyenDucDuy050/laptrinhHT
- Tai lieu yeu cau: manual ls(1) NetBSD 10.1 do giang vien cung cap.

## 1. Muc tieu

Tu cai dat tap con lenh ls bang C, doc thu muc va metadata truc tiep qua API UNIX.
Chuong trinh khong goi system("ls"), popen() hay lenh ls cua he dieu hanh.
Ket qua mac dinh co mot ten tren moi dong. Khong co doi so thi doc thu muc hien tai.
Khi co nhieu doi so, file duoc in truoc thu muc; hai nhom duoc sap xep rieng.

## 2. Bien dich va chay tren Ubuntu

Can C compiler, make va Python 3 de chay test:

```sh
sudo apt install build-essential python3 git
git clone https://github.com/NguyenDucDuy050/laptrinhHT.git
cd laptrinhHT
make
./ls
./ls -laF .
./ls -n /dev/null
./ls -R .
make test
```

Neu da co ban sao duoc chuyen qua SSH trong may ao:

```sh
cd ~/laptrinhHT
make
make test
./ls -laF
```

`make clean` xoa executable va object files. Dung `./ls` de chay bai lam;
`ls` khong co `./` thuong la chuong trinh cua he dieu hanh.

### Chay tu PowerShell qua SSH vao VirtualBox

May ao Ubuntu can dang chay. Tren may Windows da cau hinh SSH key, chay:

```powershell
ssh -i "$env:USERPROFILE\.ssh\codex_ubuntu" -p 2222 NguyenDucDuy@127.0.0.1
```

Sau khi dang nhap Ubuntu:

```sh
cd /media/sf_LapTrinhHeThong
make
make test
./ls -laF
```

Hoac goi ca build/test tu PowerShell bang mot lenh:

```powershell
ssh -i "$env:USERPROFILE\.ssh\codex_ubuntu" -p 2222 NguyenDucDuy@127.0.0.1 'cd /media/sf_LapTrinhHeThong && make && make test && ./ls -laF'
```

Source nam ngay thu muc goc, khong con thu muc con `ls_midterm`.
Shared Folder da hoat dong va tai khoan da thuoc nhom vboxsf.
Test tao fixture trong thu muc tam Linux de kiem tra quyen, symlink va socket
tren filesystem Linux, khong phu thuoc kha nang mo phong cua Shared Folder.

## 3. Chuc nang

| Tuy chon | Hanh vi |
|---|---|
| -A | Hien file an, tru . va ..; tu dong bat voi effective UID 0 |
| -a | Hien ca . va .. |
| -c | Chon ctime cho -t va -l |
| -d | Hien chinh thu muc, khong liet ke noi dung |
| -F | Them /, *, @, =, \|, % theo loai file |
| -f | Giu thu tu doc/doi so, khong sap xep |
| -h | Dinh dang size/block de doc, don vi co so 1024 |
| -i | In inode |
| -k | Don vi block 1024 byte |
| -l | Mode, link count, owner, group, size, thoi gian, ten |
| -n | Long format voi UID/GID so |
| -q | Thay ky tu khong in duoc bang ? |
| -R | Liet ke de quy |
| -r | Dao ket qua sap xep |
| -S | Kich thuoc lon truoc |
| -s | So block thuc su duoc cap phat |
| -t | Thoi gian moi truoc; cung thoi gian thi theo ten |
| -u | Chon atime cho -t va -l |
| -w | In nguyen ten file |

Cap -q/-w, -l/-n, -c/-u, -R/-d, -h/-k: tuy chon sau cung co hieu luc.
-s dung st_blocks (don vi goc 512 byte), khong suy ra tu st_size.
-l in total truoc noi dung moi thu muc; -s cung in total khi stdout la terminal.
BLOCKSIZE thay doi don vi block neu khong co -h/-k; TZ thay doi mui gio.
Owner/group khong tra duoc ten thi dung ID so. Device file in major/minor.
Symlink trong long format in `ten -> dich`; quyen co xu ly s/S va t/T.

## 4. Thiet ke

| Module | Trach nhiem |
|---|---|
| main.c | Khoi tao locale, terminal, root default, BLOCKSIZE, exit status |
| options.c/.h | getopt va quy tac ghi de tuy chon |
| listing.c/.h | opendir/readdir, lstat, danh sach dong, qsort, de quy, in cot |
| format.c/.h | Mode, suffix, time, human size, block unit, ky tu khong in duoc |
| tests/test_ls.py | Black-box test trong thu muc tam, don dep sau test |

Danh sach Entry cap phat dong, tang gap doi dung luong khi can.
qsort sap xep O(n log n); -f giu thu tu dau vao. Duong dan va dich symlink
cap phat dong, khong cat ten theo mot buffer PATH_MAX co dinh.
Moi lan doc thu muc deu dong DIR truoc khi xuong thu muc con.
De quy bo qua . va .., khong theo symlink trong noi dung thu muc, kiem tra
(dev, ino) cua cac thu muc cha de phat hien chu trinh.

## 5. Loi va gioi han

Loi duong dan/quyen/readdir/readlink duoc ghi stderr; chuong trinh tiep tuc
voi cac doi so khac khi co the. Thanh cong tra 0, co loi tra so duong.
Loi cap phat bo nho dung chuong trinh voi thong bao; kiem tra tran khi tang
mang va ghep duong dan. Kiem tra loi ghi stdout truoc khi ket thuc.

Manual rut gon co mot so diem khong mo ta day du. Quy uoc duoc chon:

- -S/-t: tuy chon sau cung chon khoa chinh; -f vo hieu sap xep.
- Symlink doi so tro den thu muc duoc theo khi khong co -d, -l/-n, -F.
  Symlink duoc tim thay ben trong thu muc khong duoc theo khi de quy.
- Thoi gian luon in thang/ngay/gio:phut nhu manual; khong them quy tac
  hien nam cua GNU ls cho file cu.
- Human-readable dung co so 1024, mot chu so thap phan khi nho hon 10.
- BLOCKSIZE chap nhan so byte va hau to K/M/G/T, b/B; gia tri sai canh bao
  va quay ve 512. Quy tac chi tiet cua getbsize khong nam trong manual rut gon.
- Gioi han de quy 256 cap de tranh tran stack; vuot gioi han bao loi.
- Whiteout/archive-state phu thuoc macro cua he dieu hanh. Linux khong cung
  cap day du cac kieu dac thu NetBSD nay; chung chua duoc kiem thu tren NetBSD.

## 6. Kiem thu

`make test` chay 14 nhom kiem thu: file an/root, sap xep va dao thu tu,
thoi gian, de quy, nhieu doi so va loi, suffix, symlink, quyen/UID/GID,
block/sparse file, total, inode/device/TZ, ky tu ten file, terminal va quyen doc.
Test quyen doc duoc bo qua khi root vi root co the bo qua quyen thu muc.
GitHub Actions build GCC va Clang voi -Werror, chay test voi user thuong va root.
Xem muc Actions cua repository de biet ket qua cua commit dang nop.

Ket qua xac minh ngay 13/10/2026:

- Ubuntu 26.04 trong VirtualBox: build voi -Werror thanh cong, 14/14 test dat.
- GitHub Actions cua commit 8a7adc1: ca GCC va Clang thanh cong. Moi job chay
  test user thuong va root (root bo qua test permission-denied theo thiet ke).
- Log: https://github.com/NguyenDucDuy050/laptrinhHT/actions/runs/38286281538

## 7. Nop bai

README nay la bao cao va da co URL repository. Source, header, Makefile,
.gitignore va test duoc commit; khong commit executable, object file hay cache.
De yeu cau ten repo `NguyenDucDuy_24IT050_midterm`; repo `laptrinhHT` duoc dung
theo lua chon cua sinh vien. Can xac nhan ten nay duoc chap nhan hoac doi ten
tren GitHub truoc khi nop. Nop bao cao/URL qua e-learning theo huong dan giang vien.
