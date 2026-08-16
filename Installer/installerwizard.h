#ifndef INSTALLERWIZARD_H
#define INSTALLERWIZARD_H

#include <QWizard>

class IntroPage;
class DirectoryPage;
class OptionsPage;
class ProgressPage;
class ConclusionPage;

class InstallerWizard : public QWizard
{
    Q_OBJECT

public:
    enum { Page_Intro, Page_Directory, Page_Options, Page_Progress, Page_Conclusion };

    InstallerWizard(QWidget *parent = nullptr);

private:
    IntroPage *introPage;
    DirectoryPage *directoryPage;
    OptionsPage *optionsPage;
    ProgressPage *progressPage;
    ConclusionPage *conclusionPage;
};

#endif // INSTALLERWIZARD_H
