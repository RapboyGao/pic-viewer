#pragma once

#include <QStringList>

class ImageCatalog
{
public:
    bool loadFromPath(const QString& path);
    void clear();

    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] int size() const;
    [[nodiscard]] int currentIndex() const;
    [[nodiscard]] QString currentPath() const;
    [[nodiscard]] QStringList paths() const;

    bool setCurrentPath(const QString& path);
    bool moveNext();
    bool movePrevious();

    static QStringList supportedExtensions();
    static bool isSupportedFile(const QString& path);

private:
    void setEntries(QStringList entries, const QString& selectedPath);

    QStringList entries_;
    int currentIndex_ = -1;
};
