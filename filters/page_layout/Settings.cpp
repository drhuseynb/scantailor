/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C) 2007-2008  Joseph Artsimovich <joseph_a@mail.ru>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Settings.h"
#include "PageId.h"
#include "Params.h"
#include "Margins.h"
#include "Alignment.h"
#include <QSizeF>
#include <QRectF>
#include <QMutex>
#include <QMutexLocker>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <algorithm>
#include <functional> // for std::greater<>

using namespace ::boost;
using namespace ::boost::multi_index;

namespace page_layout
{

class Settings::Item
{
public:
	PageId pageId;
	Margins hardMarginsMM;
	QRectF contentRect;
	QSizeF contentSizeMM;
	Alignment alignment;
	
	Item(PageId const& page_id, Margins const& hard_margins_mm,
		QRectF const& content_rect, QSizeF const& content_size_mm,
		Alignment const& alignment);
	
	double hardWidthMM() const;
	
	double hardHeightMM() const;
};


class Settings::ModifyMargins
{
public:
	ModifyMargins(Margins const& margins_mm) : m_marginsMM(margins_mm) {}
	
	void operator()(Item& item) {
		item.hardMarginsMM = m_marginsMM;
	}
private:
	Margins m_marginsMM;
};


class Settings::ModifyAlignment
{
public:
	ModifyAlignment(Alignment const& alignment) : m_alignment(alignment) {}
	
	void operator()(Item& item) {
		item.alignment = m_alignment;
	}
private:
	Alignment m_alignment;
};


class Settings::ModifyContentSize
{
public:
	ModifyContentSize(QSizeF const& content_size_mm)
	: m_contentSizeMM(content_size_mm) {}
	
	void operator()(Item& item) {
		item.contentSizeMM = m_contentSizeMM;
	}
private:
	QSizeF m_contentSizeMM;
};


class Settings::Impl
{
public:
	Impl();
	
	~Impl();
	
	std::auto_ptr<Params> getPageParams(PageId const& page_id) const;
	
	Margins getHardMarginsMM(PageId const& page_id) const;
	
	void setHardMarginsMM(PageId const& page_id, Margins const& margins_mm);
	
	Alignment getPageAlignment(PageId const& page_id) const;
	
	void setPageAlignment(PageId const& page_id, Alignment const& alignment);
	
	void setContentZone(
		PageId const& page_id,
		QRectF const& content_rect, QSizeF const& content_size_mm);
	
	QSizeF getAggregateHardSizeMM() const;
	
	QSizeF getAggregateHardSizeMM(
		PageId const& page_id, QSizeF const& hard_size_mm) const;
	
	PageId findWidestPage() const;
	
	PageId findTallestPage() const;
private:
	class DescWidthTag;
	class DescHeightTag;
	
	typedef multi_index_container<
		Item,
		indexed_by<
			ordered_unique<member<Item, PageId, &Item::pageId> >,
			ordered_non_unique<
				tag<DescWidthTag>,
				const_mem_fun<Item, double, &Item::hardWidthMM>,
				std::greater<double>
			>,
			ordered_non_unique<
				tag<DescHeightTag>,
				const_mem_fun<Item, double, &Item::hardHeightMM>,
				std::greater<double>
			>
		>
	> Container;
	
	typedef Container::index<DescWidthTag>::type DescWidthOrder;
	typedef Container::index<DescHeightTag>::type DescHeightOrder;
	
	mutable QMutex m_mutex;
	Container m_items;
	DescWidthOrder& m_descWidthOrder;
	DescHeightOrder& m_descHeightOrder;
	QSizeF const m_zeroSize;
	Margins const m_defaultHardMarginsMM;
	Alignment const m_defaultAlignment;
};


/*=============================== Settings ==================================*/

Settings::Settings()
:	m_ptrImpl(new Impl())
{
}

Settings::~Settings()
{
}

std::auto_ptr<Params>
Settings::getPageParams(PageId const& page_id) const
{
	return m_ptrImpl->getPageParams(page_id);
}

Margins
Settings::getHardMarginsMM(PageId const& page_id) const
{
	return m_ptrImpl->getHardMarginsMM(page_id);
}

void
Settings::setHardMarginsMM(PageId const& page_id, Margins const& margins_mm)
{
	m_ptrImpl->setHardMarginsMM(page_id, margins_mm);
}

Alignment
Settings::getPageAlignment(PageId const& page_id) const
{
	return m_ptrImpl->getPageAlignment(page_id);
}

void
Settings::setPageAlignment(PageId const& page_id, Alignment const& alignment)
{
	m_ptrImpl->setPageAlignment(page_id, alignment);
}

void
Settings::setContentZone(
	PageId const& page_id, QRectF const& content_rect,
	QSizeF const& content_size_mm)
{
	m_ptrImpl->setContentZone(page_id, content_rect, content_size_mm);
}

QSizeF
Settings::getAggregateHardSizeMM() const
{
	return m_ptrImpl->getAggregateHardSizeMM();
}

QSizeF
Settings::getAggregateHardSizeMM(
	PageId const& page_id, QSizeF const& hard_size_mm) const
{
	return m_ptrImpl->getAggregateHardSizeMM(page_id, hard_size_mm);
}

PageId
Settings::findWidestPage() const
{
	return m_ptrImpl->findWidestPage();
}

PageId
Settings::findTallestPage() const
{
	return m_ptrImpl->findTallestPage();
}


/*============================== Settings::Item =============================*/

Settings::Item::Item(
	PageId const& page_id, Margins const& hard_margins_mm,
	QRectF const& content_rect, QSizeF const& content_size_mm,
	Alignment const& align)
:	pageId(page_id),
	hardMarginsMM(hard_margins_mm),
	contentRect(content_rect),
	contentSizeMM(content_size_mm),
	alignment(align)
{
}

double
Settings::Item::hardWidthMM() const
{
	return contentSizeMM.width() + hardMarginsMM.left() + hardMarginsMM.right();
}

double
Settings::Item::hardHeightMM() const
{
	return contentSizeMM.height() + hardMarginsMM.top() + hardMarginsMM.bottom();
}


/*============================= Settings::Impl ==============================*/

Settings::Impl::Impl()
:	m_items(),
	m_descWidthOrder(m_items.get<DescWidthTag>()),
	m_descHeightOrder(m_items.get<DescHeightTag>()),
	m_zeroSize(0.0, 0.0),
	m_defaultHardMarginsMM(10.0, 5.0, 10.0, 5.0),
	m_defaultAlignment(Alignment::VCENTER, Alignment::HCENTER)
{
}

Settings::Impl::~Impl()
{
}

std::auto_ptr<Params>
Settings::Impl::getPageParams(PageId const& page_id) const
{
	QMutexLocker const locker(&m_mutex);
	
	Container::iterator const it(m_items.find(page_id));
	if (it == m_items.end()) {
		return std::auto_ptr<Params>();
	}
	
	return std::auto_ptr<Params>(
		new Params(
			it->hardMarginsMM, it->contentRect,
			it->contentSizeMM, it->alignment
		)
	);
}

Margins
Settings::Impl::getHardMarginsMM(PageId const& page_id) const
{
	QMutexLocker const locker(&m_mutex);
	
	Container::iterator const it(m_items.find(page_id));
	if (it == m_items.end()) {
		return m_defaultHardMarginsMM;
	} else {
		return it->hardMarginsMM;
	}
}

void
Settings::Impl::setHardMarginsMM(
	PageId const& page_id, Margins const& margins_mm)
{
	QMutexLocker const locker(&m_mutex);
	
	Container::iterator const it(m_items.lower_bound(page_id));
	if (it == m_items.end() || page_id < it->pageId) {
		Item const item(
			page_id, margins_mm, QRectF(),
			m_zeroSize, m_defaultAlignment
		);
		m_items.insert(it, item);
	} else {
		m_items.modify(it, ModifyMargins(margins_mm));
	}
}

Alignment
Settings::Impl::getPageAlignment(PageId const& page_id) const
{
	QMutexLocker const locker(&m_mutex);
	
	Container::iterator const it(m_items.find(page_id));
	if (it == m_items.end()) {
		return m_defaultAlignment;
	} else {
		return it->alignment;
	}
}

void
Settings::Impl::setPageAlignment(
	PageId const& page_id, Alignment const& alignment)
{
	QMutexLocker const locker(&m_mutex);
	
	Container::iterator const it(m_items.lower_bound(page_id));
	if (it == m_items.end() || page_id < it->pageId) {
		Item const item(
			page_id, m_defaultHardMarginsMM,
			QRectF(), m_zeroSize, alignment
		);
		m_items.insert(it, item);
	} else {
		m_items.modify(it, ModifyAlignment(alignment));
	}
}

void
Settings::Impl::setContentZone(
	PageId const& page_id,
	QRectF const& content_rect, QSizeF const& content_size_mm)
{
	QMutexLocker const locker(&m_mutex);
	
	Container::iterator const it(m_items.lower_bound(page_id));
	if (it == m_items.end() || page_id < it->pageId) {
		Item const item(
			page_id, m_defaultHardMarginsMM,
			content_rect, content_size_mm, m_defaultAlignment
		);
		m_items.insert(it, item);
	} else {
		m_items.modify(it, ModifyContentSize(content_size_mm));
	}
}

QSizeF
Settings::Impl::getAggregateHardSizeMM() const
{
	QMutexLocker const locker(&m_mutex);
	
	if (m_items.empty()) {
		return QSizeF(0.0, 0.0);
	}
	
	Item const& max_width_item = *m_descWidthOrder.begin();
	Item const& max_height_item = *m_descHeightOrder.begin();
	
	double const width = max_width_item.hardWidthMM();
	double const height = max_height_item.hardHeightMM();
	
	return QSizeF(width, height);
}

QSizeF
Settings::Impl::getAggregateHardSizeMM(
	PageId const& page_id, QSizeF const& hard_size_mm) const
{
	QMutexLocker const locker(&m_mutex);
	
	if (m_items.empty()) {
		return QSizeF(0.0, 0.0);
	}
	
	double width = 0.0;
	
	{
		DescWidthOrder::iterator it(m_descWidthOrder.begin());
		if (it->pageId != page_id) {
			width = it->hardWidthMM();
		} else {
			++it;
			if (it == m_descWidthOrder.end()) {
				width = hard_size_mm.width();
			} else {
				width = std::max(
					hard_size_mm.width(), it->hardWidthMM()
				);
			}
		}
	}
	
	double height = 0.0;
	
	{
		DescHeightOrder::iterator it(m_descHeightOrder.begin());
		if (it->pageId != page_id) {
			height = it->hardHeightMM();
		} else {
			++it;
			if (it == m_descHeightOrder.end()) {
				height = hard_size_mm.height();
			} else {
				height = std::max(
					hard_size_mm.height(), it->hardHeightMM()
				);
			}
		}
	}
	
	return QSizeF(width, height);
}

PageId
Settings::Impl::findWidestPage() const
{
	QMutexLocker const locker(&m_mutex);
	
	if (m_items.empty()) {
		return PageId();
	}
	
	return m_descWidthOrder.begin()->pageId;
}

PageId
Settings::Impl::findTallestPage() const
{
	QMutexLocker const locker(&m_mutex);
	
	if (m_items.empty()) {
		return PageId();
	}
	
	return m_descHeightOrder.begin()->pageId;
}

} // namespace page_layout