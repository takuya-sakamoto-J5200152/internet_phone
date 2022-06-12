#include <arpa/inet.h>
#include <assert.h>
#include <complex.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define SIZE 1024

void *func1(void *);
void *func2(void *);
void die(char *s) {
    perror(s);
    exit(1);
}
typedef short sample_t;
long fn = 44100;
long f1 = 0;
long f2 = 10000;
void sample_to_complex(sample_t *s, complex double *X, long n);
void complex_to_sample(complex double *X, sample_t *s, long n);
void fft_r(complex double *x, complex double *y, long n, complex double w);
void fft(complex double *x, complex double *y, long n);
void ifft(complex double *y, complex double *x, long n);
int pow2check(long N);
void bandpass(complex double *Y, long n, long f1, long f2);
int shutout(sample_t *buf, int n);
void shut_noise(complex double *Y, complex double *Y_pre, long n);
void serv_process(int argc, char *argv[], int *s, int *ss);
void client_process(int argc, char *argv[], int *s);

int main(int argc, char *argv[]) {
    int s = 0;
    int ss = 0;
    if (atoi(argv[1]) == 0) {
        serv_process(argc, argv, &s, &ss);
    } else if (atoi(argv[1]) == 1) {
        client_process(argc, argv, &s);
    } else {
        printf("second argument must be 0 or 1\n");
        exit(1);
    }
    /*if (argc != 3) {
        printf("two argument is needed");
        return -1;
    }

    int port = atoi(argv[2]);

    int ss = socket(PF_INET, SOCK_STREAM, 0);
    if (ss == -1) die("socket");

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(ss, (struct sockaddr *)&addr, sizeof(addr)) == -1) die("bind");

    if (listen(ss, 10) == -1) die("listen");

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(struct sockaddr_in);
    int s = accept(ss, (struct sockaddr *)&client_addr, &len);
    if (s == -1) die("accept");*/

    pthread_t thread1, thread2;
    int ret1, ret2;
    ret1 = pthread_create(&thread1, NULL, (void *)func1, &s);
    if (ret1 != 0) die("pthread_create");
    ret2 = pthread_create(&thread2, NULL, (void *)func2, &s);
    if (ret2 != 0) die("pthread_create");

    ret1 = pthread_join(thread1, NULL);
    if (ret1 != 0) die("pthread_join");
    ret2 = pthread_join(thread2, NULL);
    if (ret2 != 0) die("pthread_join");
    close(ss);
}

void *func1(void *s_) {
    int s = *(int *)s_;
    FILE *fp = NULL;
    char *cmdline = "rec -t raw -b 16 -c 1 -e s -r 44100 -";
    if ((fp = popen(cmdline, "r")) == NULL) {
        err(EXIT_FAILURE, "%s", cmdline);
    }
    // complex double * X = calloc(sizeof(complex double), SIZE);
    // complex double * Y = calloc(sizeof(complex double), SIZE);
    complex double *Y_pre = calloc(sizeof(complex double), SIZE);
    complex double *X = calloc(sizeof(complex double), SIZE);
    complex double *Y = calloc(sizeof(complex double), SIZE);
    while (1) {
        sample_t buf[8192] = {0};  // 0づめあやしい
        int n = fread(buf, sizeof(sample_t), SIZE, fp);
        if(shutout(buf,SIZE)) continue;
        for (int i = n; i < SIZE; i++) {
            X[i] = 0;
            Y[i] = 0;
        }
        sample_to_complex(buf, X, SIZE);
        fft(X, Y, SIZE);

        //shut_noise(Y, Y_pre, SIZE);
        // bandpass(Y, SIZE, f1, f2);
        ifft(Y, X, SIZE);
        complex_to_sample(X, buf, SIZE);

        int set = send(s, buf, sizeof(sample_t) * n, 0);
        if (set == -1) die("send");
        if (set == 0) {
            shutdown(s, SHUT_WR);
            break;
        }
    }
    free(X);
    free(Y);
    free(Y_pre);
    (void)pclose(fp);
    return 0;
}

void *func2(void *s_) {
    int s = *(int *)s_;
    while (1) {
        sample_t buf2[SIZE] = {0};
        int n = recv(s, buf2, sizeof(sample_t) * SIZE, 0);

        if (n == -1)
            die("recv");
        else if (n != 0) {
            write(1, buf2, n);
        } else {
            shutdown(s, SHUT_WR);
            break;
        }
    }
    return 0;
}

void sample_to_complex(sample_t *s, complex double *X, long n) {
    long i;
    for (i = 0; i < n; i++) X[i] = s[i];
}

/* 複素数を標本(整数)へ変換. 虚数部分は無視 */
void complex_to_sample(complex double *X, sample_t *s, long n) {
    long i;
    for (i = 0; i < n; i++) {
        s[i] = creal(X[i]);
    }
}

/* 高速(逆)フーリエ変換;
   w は1のn乗根.
   フーリエ変換の場合   偏角 -2 pi / n
   逆フーリエ変換の場合 偏角  2 pi / n
   xが入力でyが出力.
   xも破壊される
 */
void fft_r(complex double *x, complex double *y, long n, complex double w) {
    if (n == 1) {
        y[0] = x[0];
    } else {
        complex double W = 1.0;
        long i;
        for (i = 0; i < n / 2; i++) {
            y[i] = (x[i] + x[i + n / 2]);             /* 偶数行 */
            y[i + n / 2] = W * (x[i] - x[i + n / 2]); /* 奇数行 */
            W *= w;
        }
        fft_r(y, x, n / 2, w * w);
        fft_r(y + n / 2, x + n / 2, n / 2, w * w);
        for (i = 0; i < n / 2; i++) {
            y[2 * i] = x[i];
            y[2 * i + 1] = x[i + n / 2];
        }
    }
}

void fft(complex double *x, complex double *y, long n) {
    long i;
    double arg = 2.0 * M_PI / n;
    complex double w = cos(arg) - 1.0j * sin(arg);
    fft_r(x, y, n, w);
    for (i = 0; i < n; i++) y[i] /= n;
}

void ifft(complex double *y, complex double *x, long n) {
    double arg = 2.0 * M_PI / n;
    complex double w = cos(arg) + 1.0j * sin(arg);
    fft_r(y, x, n, w);
}

int pow2check(long N) {
    long n = N;
    while (n > 1) {
        if (n % 2) return 0;
        n = n / 2;
    }
    return 1;
}

void bandpass(complex double *Y, long n, long f1, long f2) {
    for (int i = 0; i < n / 2; ++i) {
        double i_n = (double)i / (double)n;
        if (i_n < (double)f1 / (double)fn || i_n > (double)f2 / (double)fn) {
            Y[i] = 0;
            Y[n - i] = 0;
        }
    }
}

int shutout(sample_t *buf, int n) {
    for (int i = 0; i < n; i++) {
        if (abs(buf[i]) > 10000) return 0;
    }
    return -1;
}

void shut_noise(complex double *Y, complex double *Y_pre, long n) {
    for (int i = 0; i < n; ++i) {
        double amp_dif = fabs(Y[i] / Y_pre[i]);
        Y_pre[i] = Y[i];
        if (amp_dif > 0.99 && amp_dif < 1.01) {
            Y[i] = 0;
        }
    }
}

void serv_process(int argc, char *argv[], int *s, int *ss) {
    if (argc != 3) {
        printf("two argument is needed");
        exit(1);
    }

    int port = atoi(argv[2]);

    *ss = socket(PF_INET, SOCK_STREAM, 0);
    if (*ss == -1) die("socket");

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(*ss, (struct sockaddr *)&addr, sizeof(addr)) == -1) die("bind");

    if (listen(*ss, 10) == -1) die("listen");

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(struct sockaddr_in);
    *s = accept(*ss, (struct sockaddr *)&client_addr, &len);
    if (*s == -1) die("accept");
}

void client_process(int argc, char *argv[], int *s) {
    if (argc != 4) {
        printf("three argument is needed");
        exit(1);
    }
    char *ip_addr = (char *)malloc(sizeof(char) * 100);
    if (ip_addr == NULL) {
        die("ip_addr");
    }
    ip_addr = argv[2];
    int port = atoi(argv[3]);

    *s = socket(PF_INET, SOCK_STREAM, 0);
    if (*s == -1) die("socket");

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    if (inet_aton(ip_addr, &addr.sin_addr) == 0) die("inet_aton");

    addr.sin_port = htons(port);
    if (connect(*s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        die("connect");
}