// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_DESERIALIZABLE_HPP
#define VSOMEIP_DESERIALIZABLE_HPP

namespace vsomeip {

class deserializer;

class deserializable {
public:
	virtual ~deserializable() {};
	virtual bool deserialize(deserializer *_from) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_SERIALIZABLE_HPP
