// This file is part of RSS Guard.
//
// Copyright (C) 2011-2016 by Martin Rotter <rotter.martinos@gmail.com>
//
// RSS Guard is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// RSS Guard is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with RSS Guard. If not, see <http://www.gnu.org/licenses/>.

#include "services/tt-rss/ttrssfeed.h"

#include "definitions/definitions.h"
#include "miscellaneous/application.h"
#include "miscellaneous/databasefactory.h"
#include "miscellaneous/iconfactory.h"
#include "miscellaneous/textfactory.h"
#include "gui/dialogs/formmain.h"
#include "services/tt-rss/definitions.h"
#include "services/tt-rss/ttrssserviceroot.h"
#include "services/tt-rss/ttrsscategory.h"
#include "services/tt-rss/gui/formeditfeed.h"
#include "services/tt-rss/network/ttrssnetworkfactory.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QPointer>


TtRssFeed::TtRssFeed(RootItem *parent)
  : Feed(parent) {
}

TtRssFeed::TtRssFeed(const QSqlRecord &record) : Feed(NULL) {
  setTitle(record.value(FDS_DB_TITLE_INDEX).toString());
  setId(record.value(FDS_DB_ID_INDEX).toInt());
  setIcon(qApp->icons()->fromByteArray(record.value(FDS_DB_ICON_INDEX).toByteArray()));
  setAutoUpdateType(static_cast<Feed::AutoUpdateType>(record.value(FDS_DB_UPDATE_TYPE_INDEX).toInt()));
  setAutoUpdateInitialInterval(record.value(FDS_DB_UPDATE_INTERVAL_INDEX).toInt());
  setCustomId(record.value(FDS_DB_CUSTOM_ID_INDEX).toInt());
}

TtRssFeed::~TtRssFeed() {
}

int TtRssFeed::messageForeignKeyId() const {
  return customId();
}

TtRssServiceRoot *TtRssFeed::serviceRoot() const {
  return qobject_cast<TtRssServiceRoot*>(getParentServiceRoot());
}

QVariant TtRssFeed::data(int column, int role) const {
  switch (role) {
    case Qt::ToolTipRole:
      if (column == FDS_MODEL_TITLE_INDEX) {
        QString auto_update_string;

        switch (autoUpdateType()) {
          case DontAutoUpdate:
            //: Describes feed auto-update status.
            auto_update_string = tr("does not use auto-update");
            break;

          case DefaultAutoUpdate:
            //: Describes feed auto-update status.
            auto_update_string = tr("uses global settings");
            break;

          case SpecificAutoUpdate:
          default:
            //: Describes feed auto-update status.
            auto_update_string = tr("uses specific settings "
                                    "(%n minute(s) to next auto-update)",
                                    0,
                                    autoUpdateRemainingInterval());
            break;
        }

        //: Tooltip for feed.
        return tr("%1"
                  "%2\n\n"
                  "Auto-update status: %3").arg(title(),
                                                description().isEmpty() ? QString() : QString('\n') + description(),
                                                auto_update_string);
      }
      else {
        return Feed::data(column, role);
      }

    default:
      return Feed::data(column, role);
  }
}

bool TtRssFeed::canBeEdited() const {
  return true;
}

bool TtRssFeed::editViaGui() {
  QPointer<FormEditFeed> form_pointer = new FormEditFeed(serviceRoot(), qApp->mainForm());

  form_pointer.data()->execForEdit(this);
  delete form_pointer.data();
  return false;
}

bool TtRssFeed::canBeDeleted() const {
  return true;
}

bool TtRssFeed::deleteViaGui() {
  if (removeItself()) {
    serviceRoot()->requestItemRemoval(this);
    return true;
  }
  else {
    return false;
  }
}

bool TtRssFeed::markAsReadUnread(RootItem::ReadStatus status) {
  QStringList ids = getParentServiceRoot()->customIDSOfMessagesForItem(this);
  TtRssUpdateArticleResponse response = serviceRoot()->network()->updateArticles(ids, UpdateArticle::Unread,
                                                                                 status == RootItem::Unread ?
                                                                                   UpdateArticle::SetToTrue :
                                                                                   UpdateArticle::SetToFalse);

  if (serviceRoot()->network()->lastError() != QNetworkReply::NoError || response.updateStatus()  != STATUS_OK) {
    return false;
  }
  else {
    return getParentServiceRoot()->markFeedsReadUnread(QList<Feed*>() << this, status);
  }
}

bool TtRssFeed::cleanMessages(bool clear_only_read) {
  return getParentServiceRoot()->cleanFeeds(QList<Feed*>() << this, clear_only_read);
}

bool TtRssFeed::editItself(TtRssFeed *new_feed_data) {
  QSqlDatabase database = qApp->database()->connection("aa", DatabaseFactory::FromSettings);
  QSqlQuery query_update(database);

  query_update.setForwardOnly(true);
  query_update.prepare("UPDATE Feeds "
                       "SET update_type = :update_type, update_interval = :update_interval "
                       "WHERE id = :id;");

  query_update.bindValue(QSL(":update_type"), (int) new_feed_data->autoUpdateType());
  query_update.bindValue(QSL(":update_interval"), new_feed_data->autoUpdateInitialInterval());
  query_update.bindValue(QSL(":id"), id());

  if (query_update.exec()) {
    setAutoUpdateType(new_feed_data->autoUpdateType());
    setAutoUpdateInitialInterval(new_feed_data->autoUpdateInitialInterval());

    return true;
  }
  else {
    return false;
  }
}

QList<Message> TtRssFeed::obtainNewMessages() {
  QList<Message> messages;
  int newly_added_messages = 0;
  int limit = MAX_MESSAGES;
  int skip = 0;

  do {
    TtRssGetHeadlinesResponse headlines = serviceRoot()->network()->getHeadlines(customId(), limit, skip,
                                                                                 true, true, false);

    if (serviceRoot()->network()->lastError() != QNetworkReply::NoError) {
      setStatus(Feed::Error);
      serviceRoot()->itemChanged(QList<RootItem*>() << this);
      return QList<Message>();
    }
    else {
      QList<Message> new_messages = headlines.messages();

      messages.append(new_messages);
      newly_added_messages = new_messages.size();
      skip += newly_added_messages;
    }
  }
  while (newly_added_messages > 0);

  return messages;
}

bool TtRssFeed::removeItself() {
  TtRssUnsubscribeFeedResponse response = serviceRoot()->network()->unsubscribeFeed(customId());

  if (response.code() == UFF_OK) {
    // Feed was removed online from server, remove local data.
    QSqlDatabase database = qApp->database()->connection(metaObject()->className(), DatabaseFactory::FromSettings);
    QSqlQuery query_remove(database);

    query_remove.setForwardOnly(true);

    // Remove all messages from this standard feed.
    query_remove.prepare(QSL("DELETE FROM Messages WHERE feed = :feed;"));
    query_remove.bindValue(QSL(":feed"), customId());

    if (!query_remove.exec()) {
      return false;
    }

    // Remove feed itself.
    query_remove.prepare(QSL("DELETE FROM Feeds WHERE custom_id = :feed;"));
    query_remove.bindValue(QSL(":feed"), customId());

    return query_remove.exec();
  }
  else {
    qWarning("TT-RSS: Unsubscribing from feed failed, received JSON: '%s'", qPrintable(response.toString()));

    return false;
  }
}
