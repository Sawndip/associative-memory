/*
    Copyright (c) 2010 Séverin Lemaignan (slemaign@laas.fr)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version
    3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>

#include <algorithm>

#include "macros.h"
#include "constants.h"

#include "oroview.h"
#include "graph.h"
#include "node.h"
#include "node_relation.h"


using namespace std;
using namespace boost;

Node::Node(const string& id, const string& label, const Node* neighbour, node_type type) :
    id(id),
    renderer(NodeRenderer(hash_value(id), label, type)),
    selected(false),
    decayTime(0.0),
    decaySpeed(1.0),
    decaying(true),
    distance_to_selected(-1),
    distance_to_selected_updated(false),
    base_charge(INITIAL_CHARGE),
    charge(INITIAL_CHARGE)
{

    //If a neighbour is given, we set our initial position close to it.
    if (neighbour != NULL)
        pos = neighbour->pos + vec2f(10.0 * (float)rand()/RAND_MAX - 5 , 10.0 * (float)rand()/RAND_MAX - 5);
    else
        pos = vec2f(100.0 * (float)rand()/RAND_MAX - 50 , 100 * (float)rand()/RAND_MAX - 50);

    speed = vec2f(0.0, 0.0);

    mass = INITIAL_MASS;
    damping = INITIAL_DAMPING;


}

bool Node::operator< (const Node& node2) const {
    return id < node2.getID();
}

const string& Node::getID() const {
    return id;
}

void Node::setColour(vec4f col) {
    renderer.setColour(col);
}

std::vector<NodeRelation>& Node::getRelations() {
    return relations;
}

vector<Node*>  Node::getConnectedNodes(){
    vector<Node*> res;

    BOOST_FOREACH(NodeRelation& rel, relations) {
        if (rel.to == this)
            res.push_back(rel.from);
        else
            res.push_back(rel.to);
    }
    return res;
}

bool Node::isConnectedTo(Node* node) {
    BOOST_FOREACH(NodeRelation& rel, relations) {
        if (rel.to == node || rel.from == node)
            return true;
    }
    return false;
}

NodeRelation& Node::addRelation(Node& to, const relation_type type, const std::string& label) {

    //When adding a new relation, we create as well an initial UNDEFINED reciprocal relation if
    //the destination node has no back relation.
    //And before adding a relation, we check there is no existing UNDEFINED relation.
    //If it's the case, we can safely replace it by a well defined relation.

//    if (type == UNDEFINED) {
//	//Add an undefined relation and return.
//	relations.push_back(NodeRelation(this, &to, UNDEFINED, ""));
//	TRACE("Added UNDEFINED back relation to " << to.getID());
//	return;
//    }

    vector<const NodeRelation*> rels = getRelationTo(to);

    relations.push_back(NodeRelation(this, &to, type, label)); //Add a new relation

    if (!to.isConnectedTo(this)){
        to.addRelation(*this, UNDEFINED, "");
    }

    if(rels.size() == 1 && rels[0]->type == UNDEFINED) { //I had already an undefined relation to the destination node

    TRACE("Will replaced an old UNDEFINED relation by a better one!");

    //**** TODO !!! *****/

    //if (edge_p != NULL)
    //    edge_p->removeReferenceRelation(*this);

    //std::remove(relations.begin(), relations.end(), *(rels[0]));
    }

    TRACE("Added relation from " << id << " to " << to.getID());

    return relations.back();

}

vector<const NodeRelation*> Node::getRelationTo(Node& node) const {

    vector<const NodeRelation*> res;

    BOOST_FOREACH(const NodeRelation& rel, relations) {
        if (rel.to == &node)
            res.push_back(&rel);
    }
    return res;
}


void Node::updateKineticEnergy() {
    kinetic_energy = mass * speed.length2();
}


void Node::step(Graph& g, float dt){

    if(!selected) {

    /** Compute here the new position of the node **/

    // Algo from Wikipedia -- http://en.wikipedia.org/wiki/Force-based_layout

    TRACE("Stepping for node " << id);

    vec2f force = vec2f(0.0, 0.0);

    coulombForce = g.coulombRepulsionFor(*this);
    force +=coulombForce;

    hookeForce = g.hookeAttractionFor(*this);
    force += hookeForce;

    TRACE("** Total force applying: Fx=" << force.x << ", Fy= " << force.y);

    speed = (speed + force * dt) * damping;
    speed.x = CLAMP(speed.x, -MAX_SPEED, MAX_SPEED);
    speed.y = CLAMP(speed.y, -MAX_SPEED, MAX_SPEED);

    updateKineticEnergy();

    //Check we have enough energy to move :)
    if (kinetic_energy > MIN_KINETIC_ENERGY) {
        pos += speed * dt;
    }

        if (decaying) decayTime += dt;
        decay();
        //Update the age of the node renderer
        renderer.increment_idle_time(dt);

    TRACE("Node " << id << " now in pos=(" << pos.x << ", " << pos.y <<")");


    //TRACE("Step computed for " << id << ". Speed is " << speed.x << ", " << speed.y << " (energy: " << kinetic_energy << ").");
    }
}

void Node::render(rendering_mode mode, OroView& env, bool debug){

#ifndef TEXT_ONLY
        if (distance_to_selected >= MAX_NODE_LEVELS) return;

        renderer.draw(pos, mode, env, distance_to_selected);

        if (debug) {
            vec4f col(1.0, 0.2, 0.2, 0.7);
            OroView::drawVector(hookeForce , pos, col);

            col = vec4f(0.2, 1.0, 0.2, 0.7);
            OroView::drawVector(coulombForce , pos, col);
        }

#endif

}

void Node::decay() {

    if(decaying) {
        float decayRatio = 1.0 - decayTime / (DECAY_TIME * 100.0 / decaySpeed);

        if (decayRatio < 0.0) {
            // End of decay period
            decayRatio = 0.0;
            decaying = false;
            decayTime = 0.0;
            decaySpeed = 1.0;
        }

        renderer.decayRatio = decayRatio;

        charge = base_charge + ((charge - base_charge) * decayRatio);
    }
}

void Node::tickle() {
    decaying = true;
    decaySpeed = 0.2;
    renderer.col = SELECTED_COLOUR;
    renderer.size = NODE_SIZE * SELECT_SIZE_FACTOR * 2.0;
    charge *= 50;
}

void Node::setSelected(bool select) {

    if ((select && selected)||
    (!select && !selected)) return;

    selected = select;
    renderer.setSelected(select);

    if(select) charge *= 20;
    else charge /= 20;

}
