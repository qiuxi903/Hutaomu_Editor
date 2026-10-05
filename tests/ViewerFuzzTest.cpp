// Hutaomu Editor - Fuzz-style robustness tests for all document viewers.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
//
// 目标：任何垃圾输入（随机字节/截断/拼接/炸弹结构）进入任何查看器都
// 不得崩溃、不得卡死、不得无界分配。全部通过即"自测不报错"。
#include <QApplication>
#include <QImage>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <random>
#include <zlib.h>

#include "app/MainWindow.h"
#include "viewers/ImageViewer.h"
#include "viewers/OfficeViewer.h"
#include "viewers/PdfViewer.h"
#include "viewers/ViewerFactory.h"
#include "viewers/ZipReader.h"

namespace {

int failures = 0;
int crashDetected = 0;
QTemporaryDir* g_dir = nullptr;

void expect(bool condition, const char* what)
{
    if (condition)
        std::printf("  PASS  %s\n", what);
    else {
        std::printf("  FAIL  %s\n", what);
        ++failures;
    }
}

// 在子进程里跑单个查看器（崩溃 => 非零退出）；父进程收集结果。
// QTemporaryDir 每次新建，路径通过 argv 传递。
int runOneViewer(const QString& filePath, const QString& kind)
{
    viewers::DocumentViewer* viewer = viewers::createViewer(filePath, nullptr);
    if (!viewer) {
        std::printf("  SKIP  %s: factory returned null\n", qPrintable(kind));
        return 0;
    }
    viewer->resize(800, 600);
    viewer->show();
    // 事件循环跑一拍（触发 showEvent/懒加载）
    QTimer::singleShot(400, qApp, &QCoreApplication::quit);
    QCoreApplication::exec();
    // 缩放压力
    for (int i = 0; i < 5; ++i) {
        viewer->zoomIn();
        viewer->zoomOut();
    }
    delete viewer;
    std::printf("  OK    %s survived\n", qPrintable(kind));
    return 0;
}

// 垃圾文件生成器
QByteArray randomBytes(size_t n, quint32 seed)
{
    std::mt19937 rng(seed);
    QByteArray b;
    b.resize(int(n));
    for (size_t i = 0; i < n; ++i)
        b[int(i)] = char(rng() & 0xFF);
    return b;
}

QByteArray pdfWithStream(const QByteArray& streamContent, const char* filter)
{
    QByteArray pdf = "%PDF-1.7\n1 0 obj\n<< /Length ";
    pdf += QByteArray::number(streamContent.size());
    if (filter && *filter)
        pdf += " /Filter " + QByteArray(filter);
    pdf += " >>\nstream\n";
    pdf += streamContent;
    pdf += "\nendstream\nendobj\n%%EOF\n";
    return pdf;
}

} // namespace

int main(int argc, char* argv[])
{
    // 分发到子进程模式：--child <path> <kind>
    if (argc >= 3 && QByteArray(argv[1]) == "--child") {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QApplication app(argc, argv);
        return runOneViewer(QString::fromUtf8(argv[2]),
                            QString::fromUtf8(argc > 3 ? argv[3] : "unknown"));
    }

    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    setvbuf(stdout, nullptr, _IONBF, 0);

    QTemporaryDir dir;
    g_dir = &dir;

    // ---- 构造对抗性输入集 ----
    struct Input { const char* name; QByteArray bytes; };
    QList<Input> inputs;

    inputs.append({ "empty", QByteArray() });
    inputs.append({ "random-64b", randomBytes(64, 1) });
    inputs.append({ "random-1mb", randomBytes(1024 * 1024, 2) });
    inputs.append({ "zeros-64kb", QByteArray(64 * 1024, '\0') });
    inputs.append({ "ff-64kb", QByteArray(64 * 1024, char(0xFF)) });

    // 截断的真实结构（PDF 头 + 半个流）
    inputs.append({ "pdf-truncated", pdfWithStream(randomBytes(100000, 3), nullptr).left(500) });
    // 空流 PDF
    inputs.append({ "pdf-empty-stream", pdfWithStream(QByteArray(), nullptr) });
    // FlateDecode 的垃圾（inflate 失败路径）
    inputs.append({ "pdf-flate-garbage", pdfWithStream(randomBytes(5000, 4), "/FlateDecode") });
    // zip bomb：高压缩比流（全 0 的 64MB -> deflate 后极小）
    {
        QByteArray zeros(64 * 1024 * 1024, '\0');
        uLongf bound = compressBound(zeros.size());
        QByteArray deflated(int(bound), '\0');
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        deflateInit(&zs, 9);
        zs.next_in = reinterpret_cast<Bytef*>(zeros.data());
        zs.avail_in = uInt(zeros.size());
        zs.next_out = reinterpret_cast<Bytef*>(deflated.data());
        zs.avail_out = uInt(bound);
        deflate(&zs, Z_FINISH);
        deflated.resize(int(zs.total_out));
        deflateEnd(&zs);
        inputs.append({ "pdf-zipbomb-64mb", pdfWithStream(deflated, "/FlateDecode") });
    }
    // 大量小流（性能/重复处理）
    {
        QByteArray many;
        for (int i = 0; i < 500; ++i)
            many += pdfWithStream("BT (x) Tj ET\n", nullptr);
        inputs.append({ "pdf-500-streams", many });
    }
    // 恶意 ZIP：EOCD 声称 65535 个条目
    {
        QByteArray eocd = QByteArrayLiteral("PK\x05\x06");
        eocd += QByteArray(18, '\0');
        eocd[int(eocd.size() - 6)] = char(0xFF); // count low byte
        eocd[int(eocd.size() - 5)] = char(0xFF);
        inputs.append({ "zip-fake-65535", eocd });
    }
    // 恶意 ZIP：声称的条目 offset 超界
    {
        QByteArray cd = QByteArrayLiteral("PK\x01\x02");
        cd += QByteArray(42, '\0');
        cd[45] = char(0x7F); // offset 高位
        QByteArray eocd = QByteArrayLiteral("PK\x05\x06");
        eocd += QByteArray(18, '\0');
        eocd[10] = 1; // count = 1
        inputs.append({ "zip-offset-oob", cd + eocd });
    }
    // 伪 PNG 头 + 垃圾
    inputs.append({ "png-garbage", QByteArrayLiteral("\x89PNG\r\n\x1a\n") + randomBytes(10000, 5) });
    // 伪 docx（PK 头 + 垃圾）
    inputs.append({ "docx-garbage", QByteArrayLiteral("PK\x03\x04") + randomBytes(10000, 6) });
    // 伪 mp4（ftyp + 垃圾）
    inputs.append({ "mp4-garbage", QByteArrayLiteral("\x00\x00\x00\x18ftypmp42") + randomBytes(5000, 7) });

    // ---- 每种输入 × 每种扩展名（工厂分派）过一遍 ----
    const char* extensions[] = { "pdf", "png", "jpg", "docx", "xlsx", "pptx", "mp4", "mp3" };

    int stormsRun = 0;
    for (const Input& input : inputs) {
        for (const char* ext : extensions) {
            const QString path = dir.filePath(
                QStringLiteral("fz_%1.%2").arg(input.name, ext));
            QFile file(path);
            file.open(QIODevice::WriteOnly);
            file.write(input.bytes);
            file.close();

            // 子进程运行（崩溃隔离）
            const QString program = QCoreApplication::applicationFilePath();
            QProcess child;
            child.setProgram(program);
            child.setArguments({ QStringLiteral("--child"), path, ext });
            child.setProcessChannelMode(QProcess::MergedChannels);
            child.start();
            const bool finished = child.waitForFinished(20000);
            const int code = finished ? child.exitCode() : -1;
            if (code != 0) {
                std::printf("  CRASH  %s as %s (code=%d timeout=%d)\n",
                            input.name, ext, code, int(!finished));
                ++crashDetected;
                ++failures;
            }
            ++stormsRun;
        }
    }
    std::printf("  INFO  %d fuzz storms run\n", stormsRun);

    expect(crashDetected == 0, "no viewer crashed on any garbage input");

    if (failures > 0) {
        std::printf("\n%d failure(s)\n", failures);
        return 1;
    }
    std::printf("\nAll fuzz robustness tests passed.\n");
    return 0;
}