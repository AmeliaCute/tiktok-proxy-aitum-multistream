#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <QString>
#include <string>
#include <vector>

#include "tiktok-streamlabs.hpp"

class TikTokSetupDialog : public QDialog 
{
	Q_OBJECT

	public:
	explicit TikTokSetupDialog(const std::string &token, QWidget *parent = nullptr);

	std::string streamServer;
	std::string streamKey;

	private slots:
	void onCategoryTextChanged(const QString &text);
	void onSuggestionClicked(QListWidgetItem *item);
	void onGoLive();
	void onSearchTimer();

	private:
	void searchCategories(const QString &query);
	void setUiBusy(bool busy);
	void validateGoLive();

	std::string m_token;

	QLabel *m_statusLabel;
	QLabel *m_usernameLabel;

	QLineEdit *m_titleEdit;
	QLineEdit *m_categoryEdit;
	QListWidget *m_suggestionsList;
	QCheckBox *m_matureCheck;
	QPushButton *m_goLiveBtn;
	QPushButton *m_cancelBtn;

	QTimer m_searchTimer;

	std::string m_selectedCategoryId;
	std::string m_selectedCategoryName;
	std::vector<TikTokCategory> m_categories;
};
