#ifndef NSTALLERWZARD_H
#define NSTALLERWZARD_H

#include <QWizard>

class ntroPage;
class DirectoryPage;
class OptionsPage;
class ProgressPage;
class ConclusionPage;

class nstallerWizard : public QWizard
{
    Q_OBJECT

public:
    enum { Page_ntro, Page_Directory, Page_Options, Page_Progress, Page_Conclusion };

    nstallerWizard(QWidget *parent = nullptr);

private:
    ntroPage *introPage;
    DirectoryPage *directoryPage;
    OptionsPage *optionsPage;
    ProgressPage *progressPage;
    ConclusionPage *conclusionPage;
};

#endif // NSTALLERWZARD_H
