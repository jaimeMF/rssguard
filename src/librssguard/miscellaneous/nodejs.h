// For license of this file, see <project-root-folder>/LICENSE.md.

#ifndef NODEJS_H
#define NODEJS_H

#include <QObject>

class Settings;

class NodeJs : public QObject {
  Q_OBJECT

  struct PackageMetadata {
    public:

      // Name of package.
      QString m_name;

      // Version description. This could be fixed version or empty
      // string (latest version) or perhaps version range.
      QString m_version;
  };

  enum class PackageStatus {
    // Package not installed.
    NotInstalled,

    // Package installed but out-of-date.
    OutOfDate,

    // Package installed and up-to-date.
    UpToDate
  };

  public:
    explicit NodeJs(Settings* settings, QObject* parent = nullptr);

    QString nodeJsExecutable() const;
    void setNodeJsExecutable(const QString& exe) const;

    QString npmExecutable() const;
    void setNpmExecutable(const QString& exe) const;

    QString packageFolder() const;
    QString processedPackageFolder() const;
    void setPackageFolder(const QString& path);

    QString nodejsVersion(const QString& nodejs_exe) const;
    QString npmVersion(const QString& npm_exe) const;

    // Checks status of package.
    //
    // NOTE: https://docs.npmjs.com/cli/v8/commands/npm-ls
    PackageStatus packageStatus(const PackageMetadata& pkg) const;

    // Installs package.
    //
    // If package is NOT installed, then it will be installed.
    // If package IS installed but out-of-date, it is updated to desired versions.
    //
    // NOTE: https://docs.npmjs.com/cli/v8/commands/npm-install
    void installUpdatePackage(const PackageMetadata& pkg);

  private:
    void installPackage(const PackageMetadata& pkg);
    void updatePackage(const PackageMetadata& pkg);

    Settings* m_settings;
};

#endif // NODEJS_H
