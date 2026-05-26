#pragma once

#include <QString>
#include <QSize>
#include <QVector>

struct PlatformPreset {
    QString id;
    QString name;
    int width;
    int height;

    double aspectRatio() const {
        return static_cast<double>(width) / static_cast<double>(height);
    }

    QSize size() const { return QSize(width, height); }
};

inline QVector<PlatformPreset> platformPresets()
{
    return {
        { "instagram_square",   "Instagram Post (1:1)",     1080, 1080 },
        { "instagram_portrait", "Instagram Portrait (4:5)", 1080, 1350 },
        { "instagram_story",    "Instagram Story/Reel (9:16)", 1080, 1920 },
        { "youtube",            "YouTube (16:9)",           1920, 1080 },
        { "tiktok",             "TikTok (9:16)",            1080, 1920 },
    };
}
