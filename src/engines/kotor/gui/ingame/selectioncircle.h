/* xoreos - A reimplementation of BioWare's Aurora engine
 *
 * xoreos is the legal property of its developers, whose names
 * can be found in the AUTHORS file distributed with this source
 * distribution.
 *
 * xoreos is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * xoreos is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with xoreos. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 *  The circle visible when selecting objects
 */

#ifndef ENGINES_KOTOR_GUI_INGAME_SELECTIONCIRCLE_H
#define ENGINES_KOTOR_GUI_INGAME_SELECTIONCIRCLE_H

#include "src/graphics/aurora/guiquad.h"

namespace Engines {

namespace KotOR {

const float kSelectionCircleSize = 64.0f;

class Situated;

class SelectionCircle {
public:
	SelectionCircle();

	// Basic visuals

	void show();
	void hide();


	void setPosition(float x, float y);
	void setHovered(bool hovered);
	void setTarget(bool target);

	void moveTo(KotORBase::Situated *situated, float &sX, float &sY);

private:
	Common::ScopedPtr<Graphics::Aurora::GUIQuad> _hoveredQuad;
	Common::ScopedPtr<Graphics::Aurora::GUIQuad> _targetQuad;

	bool _hovered { false }; ///< Is this selection circle being hovered over?
	bool _target { false }; ///< Is the object below this selection circle the target?
	bool _visible { false };
};

} // End of namespace KotOR

} // End of namespace Engines

#endif // ENGINES_KOTOR_GUI_INGAME_SELECTIONCIRCLE_H
