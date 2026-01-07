#include "MainWindow.h"
#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>

class LoginDialog : public QDialog {
public:
    QLineEdit *userEdit;
    QLineEdit *passEdit;
    QString username;
    QString password;

    LoginDialog() {
        setWindowTitle("Monitor Login");
        setFixedSize(300, 150);
        setStyleSheet("background-color: #333; color: white;");

        QVBoxLayout *layout = new QVBoxLayout(this);

        userEdit = new QLineEdit(this);
        userEdit->setPlaceholderText("Username (Optional)");
        userEdit->setStyleSheet("padding: 5px; background: #444; border: 1px solid #555; color: white;");

        passEdit = new QLineEdit(this);
        passEdit->setPlaceholderText("Password");
        passEdit->setEchoMode(QLineEdit::Password);
        passEdit->setStyleSheet("padding: 5px; background: #444; border: 1px solid #555; color: white;");

        QPushButton *loginBtn = new QPushButton("Login", this);
        loginBtn->setStyleSheet("background-color: #007acc; padding: 6px; font-weight: bold; border-radius: 4px;");

        layout->addWidget(new QLabel("Enter Credentials:"));
        layout->addWidget(userEdit);
        layout->addWidget(passEdit);
        layout->addWidget(loginBtn);

        connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::accept);
    }

    void accept() override {
        username = userEdit->text();
        password = passEdit->text();
        QDialog::accept();
    }
};

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setStyle("Fusion"); 
    
    
    LoginDialog login;
    if (login.exec() == QDialog::Accepted) {
        
        MainWindow w(login.username, login.password);
        w.show();
        return a.exec();
    }
    
    return 0;
}