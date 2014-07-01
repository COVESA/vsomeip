// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERIALIZABLE_HPP
#define VSOMEIP_SERIALIZABLE_HPP

namespace vsomeip {

class serializer;

class serializable {
public:
	virtual ~serializable() {};
	virtual bool serialize(serializer *_to) const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_SERIALIZABLE_HPP
