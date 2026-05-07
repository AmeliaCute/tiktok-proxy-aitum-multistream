#include "tiktok-setup-dialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QApplication>

TikTokSetupDialog::TikTokSetupDialog(const std::string &token, QWidget *parent) : QDialog(parent), m_token(token)
{
	setWindowTitle("TikTok Live Setup");
	setMinimumWidth(460);
	setModal(true);

	auto *mainLayout = new QVBoxLayout(this);
	mainLayout->setSpacing(12);

	auto *accountGroup = new QGroupBox("Account");
	auto *accountLayout = new QFormLayout(accountGroup);

	m_usernameLabel = new QLabel("Loading…");
	m_statusLabel = new QLabel("");
	accountLayout->addRow("Username:", m_usernameLabel);
	accountLayout->addRow("Status:", m_statusLabel);
	mainLayout->addWidget(accountGroup);

	auto *detailsGroup = new QGroupBox("Stream Details");
	auto *detailsLayout = new QVBoxLayout(detailsGroup);
	auto *form = new QFormLayout;

	m_titleEdit = new QLineEdit;
	m_titleEdit->setPlaceholderText("Enter stream title…");
	form->addRow("Title:", m_titleEdit);

	m_categoryEdit = new QLineEdit;
	m_categoryEdit->setPlaceholderText("Search game / category…");
	form->addRow("Category:", m_categoryEdit);

	detailsLayout->addLayout(form);

	m_suggestionsList = new QListWidget;
	m_suggestionsList->setMaximumHeight(140);
	m_suggestionsList->setVisible(false);
	detailsLayout->addWidget(m_suggestionsList);

	m_matureCheck = new QCheckBox("Enable mature content");
	detailsLayout->addWidget(m_matureCheck);

	mainLayout->addWidget(detailsGroup);

	auto *btnRow = new QHBoxLayout;
	m_cancelBtn = new QPushButton("Cancel");
	m_cancelBtn->setFixedHeight(32);
	m_goLiveBtn = new QPushButton("🔴  Go Live");
	m_goLiveBtn->setFixedHeight(32);
	m_goLiveBtn->setEnabled(false);
	m_goLiveBtn->setStyleSheet("font-weight: bold;");
	btnRow->addWidget(m_cancelBtn);
	btnRow->addStretch();
	btnRow->addWidget(m_goLiveBtn);
	mainLayout->addLayout(btnRow);

	connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
	connect(m_goLiveBtn, &QPushButton::clicked, this, &TikTokSetupDialog::onGoLive);
	connect(m_titleEdit, &QLineEdit::textChanged, this, [this] { validateGoLive(); });
	connect(m_categoryEdit, &QLineEdit::textChanged, this, &TikTokSetupDialog::onCategoryTextChanged);
	connect(m_suggestionsList, &QListWidget::itemClicked, this, &TikTokSetupDialog::onSuggestionClicked);

	m_searchTimer.setSingleShot(true);
	m_searchTimer.setInterval(350);
	connect(&m_searchTimer, &QTimer::timeout, this, &TikTokSetupDialog::onSearchTimer);

	auto *watcher = new QFutureWatcher<TikTokAccountInfo>(this);
	connect(watcher, &QFutureWatcher<TikTokAccountInfo>::finished, this,
		[this, watcher]
		{
			auto info = watcher->result();
			if (!info.error.empty()) 
			{
				m_usernameLabel->setText("(error: " + QString::fromStdString(info.error) + ")");
			} 
			else
			{
				m_usernameLabel->setText(QString::fromStdString(info.username));
				m_statusLabel->setText(QString::fromStdString(info.status));
				if (!info.can_go_live) 
				{
					m_goLiveBtn->setEnabled(false);
					m_goLiveBtn->setToolTip("Your account does not have TikTok LIVE access.");
					m_statusLabel->setStyleSheet("color: red;");
				}
			}
			watcher->deleteLater();
		});

	auto token = m_token;
	watcher->setFuture(QtConcurrent::run([token] { return TikTokStreamlabs::getAccountInfo(token); }));
}

void TikTokSetupDialog::onCategoryTextChanged(const QString &text)
{
	if (text.trimmed() != QString::fromStdString(m_selectedCategoryName)) 
	{
		m_selectedCategoryId.clear();
		m_selectedCategoryName.clear();
		validateGoLive();
	}

	if (text.trimmed().isEmpty()) {
		m_suggestionsList->clear();
		m_suggestionsList->setVisible(false);
		m_searchTimer.stop();
		return;
	}

	m_searchTimer.start();
}

void TikTokSetupDialog::onSearchTimer()
{
	searchCategories(m_categoryEdit->text().trimmed());
}

void TikTokSetupDialog::searchCategories(const QString &query)
{
	if (query.isEmpty())
		return;

	setUiBusy(true);
	auto token = m_token;
	auto q = query.toStdString();

	auto *watcher = new QFutureWatcher<std::vector<TikTokCategory>>(this);
	connect(watcher, &QFutureWatcher<std::vector<TikTokCategory>>::finished, this,
		[this, watcher] 
		{
			m_categories = watcher->result();
			m_suggestionsList->clear();
			for (auto &cat : m_categories) m_suggestionsList->addItem(QString::fromStdString(cat.name));

			m_suggestionsList->setVisible(!m_categories.empty());
			setUiBusy(false);
			watcher->deleteLater();
		});

	watcher->setFuture(QtConcurrent::run([token, q] { return TikTokStreamlabs::searchCategories(token, q); }));
}

void TikTokSetupDialog::onSuggestionClicked(QListWidgetItem *item)
{
	int row = m_suggestionsList->row(item);
	if (row < 0 || row >= (int)m_categories.size())
		return;

	m_selectedCategoryId = m_categories[row].id;
	m_selectedCategoryName = m_categories[row].name;

	m_categoryEdit->blockSignals(true);
	m_categoryEdit->setText(QString::fromStdString(m_selectedCategoryName));
	m_categoryEdit->blockSignals(false);

	m_suggestionsList->clear();
	m_suggestionsList->setVisible(false);

	validateGoLive();
}

void TikTokSetupDialog::onGoLive()
{
	setUiBusy(true);

	auto token = m_token;
	auto title = m_titleEdit->text().toStdString();
	auto catId = m_selectedCategoryId;
	bool mature = m_matureCheck->isChecked();

	auto *watcher = new QFutureWatcher<TikTokStreamInfo>(this);
	connect(watcher, &QFutureWatcher<TikTokStreamInfo>::finished, this,
		[this, watcher] 
		{
			auto info = watcher->result();
			setUiBusy(false);
			if (!info.success)  QMessageBox::critical(this, "TikTok Live Error", "Failed to start live session:\n" + QString::fromStdString(info.error));
			else 
			{
				streamServer = info.server;
				streamKey = info.key;
				accept();
			}
			watcher->deleteLater();
		});

	watcher->setFuture(QtConcurrent::run([token, title, catId, mature] { return TikTokStreamlabs::startStream(token, title, catId, mature); }));
}

void TikTokSetupDialog::setUiBusy(bool busy)
{
	m_titleEdit->setEnabled(!busy);
	m_categoryEdit->setEnabled(!busy);
	m_matureCheck->setEnabled(!busy);
	m_goLiveBtn->setText(busy ? "Please wait :3" : "🔴  Go Live");
	if (!busy) validateGoLive();
	else m_goLiveBtn->setEnabled(false);

	QApplication::processEvents();
}

void TikTokSetupDialog::validateGoLive()
{
	bool ok = !m_titleEdit->text().trimmed().isEmpty() && !m_selectedCategoryId.empty();
	m_goLiveBtn->setEnabled(ok);
}
