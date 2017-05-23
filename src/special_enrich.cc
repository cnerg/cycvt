// Implements the SEnrichment class
#include "special_enrich.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <vector>

#include <boost/lexical_cast.hpp>

namespace cycvt {

std::pair<cyclus::Material::Ptr, cyclus::Material::Ptr> equivalent_u8(
    cyclus::Material::Ptr mat, std::map<cyclus::Nuc, double> ux) {
  double initial_q = mat->quantity();
  cyclus::Material::Ptr equiv_mat =
      cyclus::Material::CreateUntracked(mat->quantity(), mat->comp());
  cyclus::Material::Ptr mat_special_nucs;

  cyclus::CompMap to_substract;
  double q_to_switch = 0;

  std::map<cyclus::Nuc, double>::iterator it;

  for (it = ux.begin(); it != ux.end(); it++) {
    cyclus::toolkit::MatQuery mq(mat);
    double nuc_i_mass = mq.mass(it->first);
    q_to_switch += nuc_i_mass;
    to_substract[it->first] = nuc_i_mass;
  }
  if (to_substract.size() != 0) {
    cyclus::Composition::Ptr c_special_nucs =
        cyclus::Composition::CreateFromMass(to_substract);
    mat_special_nucs =
        cyclus::Material::CreateUntracked(q_to_switch, c_special_nucs);
    equiv_mat->ExtractComp(q_to_switch, c_special_nucs);

    cyclus::CompMap to_add;
    to_add[922380000] = q_to_switch;
    cyclus::Composition::Ptr c_to_add =
        cyclus::Composition::CreateFromMass(to_add);
    equiv_mat->Absorb(cyclus::Material::CreateUntracked(q_to_switch, c_to_add));
  } else {
    return std::pair<cyclus::Material::Ptr, cyclus::Material::Ptr>(
        equiv_mat, mat_special_nucs);
  }

  if (equiv_mat->quantity() != mat->quantity()) {
    std::cout << "Oupsy !!" << std::endl;
  }
  
  // return a pair containing the equivalent material replacing all special
  // nucs but U-238, and the replaced material containing the special nucs.
  return std::pair<cyclus::Material::Ptr, cyclus::Material::Ptr>(
      equiv_mat, mat_special_nucs);
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SEnrichment::SEnrichment(cyclus::Context* ctx)
    : cyclus::Facility(ctx),
      tails_assay(0),
      swu_capacity(0),
      max_enrich(1),
      initial_feed(0),
      feed_commod(""),
      feed_recipe(""),
      product_commod(""),
      tails_commod(""),
      order_prefs(true) {}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SEnrichment::~SEnrichment() {}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string SEnrichment::str() {
  std::stringstream ss;
  ss << cyclus::Facility::str() << " with enrichment facility parameters:"
     << " * SWU capacity: " << SwuCapacity()
     << " * Tails assay: " << tails_assay << " * Feed assay: " << FeedAssay()
     << " * Input cyclus::Commodity: " << feed_commod
     << " * Output cyclus::Commodity: " << product_commod
     << " * Tails cyclus::Commodity: " << tails_commod;
  return ss.str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SEnrichment::Build(cyclus::Agent* parent) {
  using cyclus::Material;

  Facility::Build(parent);
  if (initial_feed > 0) {
    inventory.Push(Material::Create(this, initial_feed,
                                    context()->GetRecipe(feed_recipe)));
  }

  LOG(cyclus::LEV_DEBUG2, "EnrFac") << "SEnrichment "
                                    << " entering the simuluation: ";
  LOG(cyclus::LEV_DEBUG2, "EnrFac") << str();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SEnrichment::Tick() { current_swu_capacity = SwuCapacity(); }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SEnrichment::Tock() {
  using cyclus::toolkit::RecordTimeSeries;
  LOG(cyclus::LEV_INFO4, "EnrFac") << prototype() << " used "
                                   << intra_timestep_swu_ << " SWU";
  RecordTimeSeries<cyclus::toolkit::ENRICH_SWU>(this, intra_timestep_swu_);
  LOG(cyclus::LEV_INFO4, "EnrFac") << prototype() << " used "
                                   << intra_timestep_feed_ << " feed";
  RecordTimeSeries<cyclus::toolkit::ENRICH_FEED>(this, intra_timestep_feed_);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr>
SEnrichment::GetMatlRequests() {
  using cyclus::Material;
  using cyclus::RequestPortfolio;
  using cyclus::Request;

  std::set<RequestPortfolio<Material>::Ptr> ports;
  RequestPortfolio<Material>::Ptr port(new RequestPortfolio<Material>());
  Material::Ptr mat = Request_();
  double amt = mat->quantity();

  if (amt > cyclus::eps_rsrc()) {
    port->AddRequest(mat, this, feed_commod);
    ports.insert(port);
  }

  return ports;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool SortBids(cyclus::Bid<cyclus::Material>* i,
              cyclus::Bid<cyclus::Material>* j) {
  cyclus::Material::Ptr mat_i = i->offer();
  cyclus::Material::Ptr mat_j = j->offer();

  cyclus::toolkit::MatQuery mq_i(mat_i);
  cyclus::toolkit::MatQuery mq_j(mat_j);

  return ((mq_i.mass(922350000) / mq_i.qty()) <=
          (mq_j.mass(922350000) / mq_j.qty()));
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Sort offers of input material to have higher preference for more
//  U-235 content
void SEnrichment::AdjustMatlPrefs(
    cyclus::PrefMap<cyclus::Material>::type& prefs) {
  using cyclus::Bid;
  using cyclus::Material;
  using cyclus::Request;

  if (order_prefs == false) {
    return;
  }

  cyclus::PrefMap<cyclus::Material>::type::iterator reqit;

  // Loop over all requests
  for (reqit = prefs.begin(); reqit != prefs.end(); ++reqit) {
    std::vector<Bid<Material>*> bids_vector;
    std::map<Bid<Material>*, double>::iterator mit;
    for (mit = reqit->second.begin(); mit != reqit->second.end(); ++mit) {
      Bid<Material>* bid = mit->first;
      bids_vector.push_back(bid);
    }
    std::sort(bids_vector.begin(), bids_vector.end(), SortBids);

    // Assign preferences to the sorted vector
    double n_bids = bids_vector.size();
    bool u235_mass = 0;

    for (int bidit = 0; bidit < bids_vector.size(); bidit++) {
      int new_pref = bidit + 1;

      // For any bids with U-235 qty=0, set pref to zero.
      if (!u235_mass) {
        cyclus::Material::Ptr mat = bids_vector[bidit]->offer();
        cyclus::toolkit::MatQuery mq(mat);
        if (mq.mass(922350000) == 0) {
          new_pref = -1;
        } else {
          u235_mass = true;
        }
      }
      (reqit->second)[bids_vector[bidit]] = new_pref;
    }  // each bid
  }    // each Material Request
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SEnrichment::AcceptMatlTrades(
    const std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                                cyclus::Material::Ptr> >& responses) {
  // see
  // http://stackoverflow.com/questions/5181183/boostshared-ptr-and-inheritance
  std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                        cyclus::Material::Ptr> >::const_iterator it;
  for (it = responses.begin(); it != responses.end(); ++it) {
    AddMat_(it->second);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::set<cyclus::BidPortfolio<cyclus::Material>::Ptr> SEnrichment::GetMatlBids(
    cyclus::CommodMap<cyclus::Material>::type& out_requests) {
  using cyclus::Bid;
  using cyclus::BidPortfolio;
  using cyclus::CapacityConstraint;
  using cyclus::Converter;
  using cyclus::Material;
  using cyclus::Request;
  using cyclus::toolkit::MatVec;

  std::set<BidPortfolio<Material>::Ptr> ports;

  if ((out_requests.count(tails_commod) > 0) && (tails.quantity() > 0)) {
    BidPortfolio<Material>::Ptr tails_port(new BidPortfolio<Material>());

    std::vector<Request<Material>*>& tails_requests =
        out_requests[tails_commod];
    std::vector<Request<Material>*>::iterator it;
    for (it = tails_requests.begin(); it != tails_requests.end(); ++it) {
      // offer bids for all tails material, keeping discrete quantities
      // to preserve possible variation in composition
      MatVec mats = tails.PopN(tails.count());
      tails.Push(mats);
      for (int k = 0; k < mats.size(); k++) {
        Material::Ptr m = mats[k];
        Request<Material>* req = *it;
        tails_port->AddBid(req, m, this);
      }
    }
    // overbidding (bidding on every offer)
    // add an overall capacity constraint
    CapacityConstraint<Material> tails_constraint(tails.quantity());
    tails_port->AddConstraint(tails_constraint);
    LOG(cyclus::LEV_INFO5, "EnrFac") << prototype()
                                     << " adding tails capacity constraint of "
                                     << tails.capacity();
    ports.insert(tails_port);
  }

  if ((out_requests.count(product_commod) > 0) && (inventory.quantity() > 0)) {
    BidPortfolio<Material>::Ptr commod_port(new BidPortfolio<Material>());

    std::vector<Request<Material>*>& commod_requests =
        out_requests[product_commod];
    std::vector<Request<Material>*>::iterator it;
    for (it = commod_requests.begin(); it != commod_requests.end(); ++it) {
      Request<Material>* req = *it;
      Material::Ptr mat = req->target();
      double request_enrich = cyclus::toolkit::UraniumAssay(mat);
      if (ValidReq(req->target()) &&
          ((request_enrich < max_enrich) ||
           (cyclus::AlmostEq(request_enrich, max_enrich)))) {
        Material::Ptr offer = Offer_(req->target());
        commod_port->AddBid(req, offer, this);
      }
    }

    Converter<Material>::Ptr sc(new SWUConverter(FeedAssay(), tails_assay, ux));
    Converter<Material>::Ptr nc(
        new NatUConverter(FeedAssay(), tails_assay, ux));
    CapacityConstraint<Material> swu(swu_capacity, sc);
    CapacityConstraint<Material> natu(inventory.quantity(), nc);
    commod_port->AddConstraint(swu);
    commod_port->AddConstraint(natu);

    LOG(cyclus::LEV_INFO5, "EnrFac")
        << prototype() << " adding a swu constraint of " << swu.capacity();
    LOG(cyclus::LEV_INFO5, "EnrFac")
        << prototype() << " adding a natu constraint of " << natu.capacity();
    ports.insert(commod_port);
  }
  return ports;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool SEnrichment::ValidReq(const cyclus::Material::Ptr mat) {
  cyclus::toolkit::MatQuery q(mat);
  double u235 = q.atom_frac(922350000);
  double u238 = q.atom_frac(922380000);
  return (u238 > 0 && u235 / (u235 + u238) > tails_assay);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SEnrichment::GetMatlTrades(
    const std::vector<cyclus::Trade<cyclus::Material> >& trades,
    std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                          cyclus::Material::Ptr> >& responses) {
  using cyclus::Material;
  using cyclus::Trade;

  intra_timestep_swu_ = 0;
  intra_timestep_feed_ = 0;

  std::vector<Trade<Material> >::const_iterator it;
  for (it = trades.begin(); it != trades.end(); ++it) {
    double qty = it->amt;
    std::string commod_type = it->bid->request()->commodity();
    Material::Ptr response;

    // Figure out whether material is tails or enriched,
    // if tails then make transfer of material
    if (commod_type == tails_commod) {
      LOG(cyclus::LEV_INFO5, "EnrFac")
          << prototype() << " just received an order"
          << " for " << it->amt << " of " << tails_commod;
      double pop_qty = std::min(qty, tails.quantity());
      response = tails.Pop(pop_qty, cyclus::eps_rsrc());
    } else {
      LOG(cyclus::LEV_INFO5, "EnrFac")
          << prototype() << " just received an order"
          << " for " << it->amt << " of " << product_commod;
      response = Enrich_(it->bid->offer(), qty);
    }
    responses.push_back(std::make_pair(*it, response));
  }

  if (cyclus::IsNegative(tails.quantity())) {
    std::stringstream ss;
    ss << "is being asked to provide more than its current inventory.";
    throw cyclus::ValueError(Agent::InformErrorMsg(ss.str()));
  }
  if (cyclus::IsNegative(current_swu_capacity)) {
    throw cyclus::ValueError("EnrFac " + prototype() +
                             " is being asked to provide more than" +
                             " its SWU capacity.");
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SEnrichment::AddMat_(cyclus::Material::Ptr mat) {
  // Elements and isotopes other than U-235, U-238 are sent directly to tails
  cyclus::CompMap cm = mat->comp()->atom();
  bool extra_u = false;
  bool other_elem = false;
  bool special_nuc = false;
  for (cyclus::CompMap::const_iterator it = cm.begin(); it != cm.end(); ++it) {
    bool extra_u_ = false;
    bool other_elem_ = false;
    if (pyne::nucname::znum(it->first) == 92) {
      if (pyne::nucname::anum(it->first) != 235 &&
          pyne::nucname::anum(it->first) != 238 && it->second > 0) {
        extra_u_ = true;
      }
    } else if (it->second > 0) {
      other_elem_ = true;
    }

    std::map<cyclus::Nuc, double>::const_iterator it2;
    for( it2 = ux.begin(); it2 != ux.end(); it2++ ){
      if(it2->first == it->first){
        extra_u_ = false;
        other_elem_ = false;
        special_nuc = true;
      }
    }
    extra_u |= extra_u_;
    other_elem |= other_elem_;
  }
  if (extra_u) {
    cyclus::Warn<cyclus::VALUE_WARNING>(
        "More than 2 isotopes of U.  "
        "Istopes other than U-235, U-238 are sent directly to tails.");
  }
  if (other_elem) {
    cyclus::Warn<cyclus::VALUE_WARNING>(
        "Non-uranium elements are "
        "sent directly to tails.");
  }
  if (special_nuc) {
    cyclus::Warn<cyclus::VALUE_WARNING>(
        "Some elements requiring a special treatment have been detected.");
  }

  LOG(cyclus::LEV_INFO5, "EnrFac") << prototype() << " is initially holding "
                                   << inventory.quantity() << " total.";

  try {
    inventory.Push(mat);
  } catch (cyclus::Error& e) {
    e.msg(Agent::InformErrorMsg(e.msg()));
    throw e;
  }

  LOG(cyclus::LEV_INFO5, "EnrFac")
      << prototype() << " added " << mat->quantity() << " of " << feed_commod
      << " to its inventory, which is holding " << inventory.quantity()
      << " total.";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
cyclus::Material::Ptr SEnrichment::Request_() {
  double qty = std::max(0.0, inventory.capacity() - inventory.quantity());
  return cyclus::Material::CreateUntracked(qty,
                                           context()->GetRecipe(feed_recipe));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
cyclus::Material::Ptr SEnrichment::Offer_(cyclus::Material::Ptr mat) {
  cyclus::toolkit::MatQuery q(mat);
  cyclus::CompMap comp;
  comp[922350000] = q.atom_frac(922350000);
  comp[922380000] = q.atom_frac(922380000);
  return cyclus::Material::CreateUntracked(
      mat->quantity(), cyclus::Composition::CreateFromAtom(comp));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
cyclus::Material::Ptr SEnrichment::Enrich_(cyclus::Material::Ptr mat,
                                          double qty) {
  using cyclus::Material;
  using cyclus::ResCast;
  using cyclus::toolkit::Assays;
  using cyclus::toolkit::UraniumAssay;
  using cyclus::toolkit::SwuRequired;
  using cyclus::toolkit::FeedQty;
  using cyclus::toolkit::TailsQty;

  // get enrichment parameters
  Assays assays(FeedAssay(), UraniumAssay(mat), tails_assay);
  double swu_req = SwuRequired(qty, assays);
  double natu_req = FeedQty(qty, assays);

  // Determine the composition of the natural uranium
  // (ie. U-235+U-238/TotalMass)
  double pop_qty = inventory.quantity();
  Material::Ptr natu_matl = inventory.Pop(pop_qty, cyclus::eps_rsrc());
  inventory.Push(natu_matl);

  // Trick the natu_mat inside the inventory

  std::pair<Material::Ptr, Material::Ptr> flip_mat =
      equivalent_u8(natu_matl, ux);
  
  cyclus::Material::Ptr equiv_natu_mat = flip_mat.first;

  cyclus::toolkit::MatQuery mq(equiv_natu_mat);
  std::set<cyclus::Nuc> nucs;
  nucs.insert(922350000);
  nucs.insert(922380000);
  double natu_frac = mq.mass_frac(nucs);
  double feed_req = natu_req / natu_frac;

  // pop amount from inventory and blob it into one material
  Material::Ptr r;
  try {
    // required so popping doesn't take out too much
    if (cyclus::AlmostEq(feed_req, inventory.quantity())) {
      r = cyclus::toolkit::Squash(inventory.PopN(inventory.count()));
    } else {
      r = inventory.Pop(feed_req, cyclus::eps_rsrc());
    }
  } catch (cyclus::Error& e) {
    NatUConverter nc(FeedAssay(), tails_assay, ux);
    std::stringstream ss;
    ss << " tried to remove " << feed_req << " from its inventory of size "
       << inventory.quantity()
       << " and the conversion of the material into natu is "
       << nc.convert(mat);
    throw cyclus::ValueError(Agent::InformErrorMsg(ss.str()));
  }

  // "enrich" it, but pull out the composition and quantity we require from the
  // blob
  cyclus::Composition::Ptr comp = mat->comp();
  Material::Ptr response = r->ExtractComp(qty, comp);

  current_swu_capacity -= swu_req;

  intra_timestep_swu_ += swu_req;
  intra_timestep_feed_ += feed_req;
  RecordSEnrichment_(feed_req, swu_req);

  // Re-Add the special nuc inside the product, and fix tails amount accordingly

  double prod_mass = response->quantity();

  cyclus::toolkit::MatQuery mq_resp(response);
  double u5_raw_enrich = mq_resp.mass_frac(922350000);
  cyclus::toolkit::MatQuery mq_flip(flip_mat.first);
  double u5_flip_enrich = mq_flip.mass_frac(922350000);

  
  std::map<cyclus::Nuc, double>::iterator it;
  for (it = ux.begin(); it != ux.end(); it++) {
    
    double nuc_i_enrich_factor =
        u5_raw_enrich / u5_flip_enrich * it->second;

    cyclus::toolkit::MatQuery mq_(natu_matl);
    double nuc_i_feed_enrich = mq_.mass(it->first) / natu_matl->quantity();
    double nuc_i_prod_enrich = nuc_i_feed_enrich * nuc_i_enrich_factor;
    double nuc_i_prod_mass = nuc_i_prod_enrich * prod_mass;

    // Remove come ux from the material pushed in the tails and add in into the
    // response, do the otherwise for the U-238 to conserve mass balance.
    cyclus::CompMap nuc_to_add;
    nuc_to_add[it->first] = nuc_i_prod_mass;
    response->Absorb(cyclus::Material::CreateUntracked(
        nuc_i_prod_mass, cyclus::Composition::CreateFromMass(nuc_to_add)));
    r->ExtractComp(nuc_i_prod_mass,
                   cyclus::Composition::CreateFromMass(nuc_to_add));

    cyclus::CompMap u8_to_remove;
    u8_to_remove[922380000] = nuc_i_prod_mass;
    r->Absorb(cyclus::Material::CreateUntracked(
        nuc_i_prod_mass, cyclus::Composition::CreateFromMass(u8_to_remove)));
    response->ExtractComp(nuc_i_prod_mass,
                          cyclus::Composition::CreateFromMass(u8_to_remove));
  }

  tails.Push(r);

  LOG(cyclus::LEV_INFO5, "EnrFac") << prototype()
                                   << " has performed an enrichment: ";
  LOG(cyclus::LEV_INFO5, "EnrFac") << "   * Feed Qty: " << feed_req;
  LOG(cyclus::LEV_INFO5, "EnrFac") << "   * Feed Assay: "
                                   << assays.Feed() * 100;
  LOG(cyclus::LEV_INFO5, "EnrFac") << "   * Product Qty: " << qty;
  LOG(cyclus::LEV_INFO5, "EnrFac") << "   * Product Assay: "
                                   << assays.Product() * 100;
  LOG(cyclus::LEV_INFO5, "EnrFac") << "   * Tails Qty: "
                                   << TailsQty(qty, assays);
  LOG(cyclus::LEV_INFO5, "EnrFac") << "   * Tails Assay: "
                                   << assays.Tails() * 100;
  LOG(cyclus::LEV_INFO5, "EnrFac") << "   * SWU: " << swu_req;
  LOG(cyclus::LEV_INFO5, "EnrFac") << "   * Current SWU capacity: "
                                   << current_swu_capacity;

  return response;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SEnrichment::RecordSEnrichment_(double natural_u, double swu) {
  using cyclus::Context;
  using cyclus::Agent;

  LOG(cyclus::LEV_DEBUG1, "EnrFac") << prototype()
                                    << " has enriched a material:";
  LOG(cyclus::LEV_DEBUG1, "EnrFac") << "  * Amount: " << natural_u;
  LOG(cyclus::LEV_DEBUG1, "EnrFac") << "  *    SWU: " << swu;

  Context* ctx = Agent::context();
  ctx->NewDatum("SEnrichments")
      ->AddVal("ID", id())
      ->AddVal("Time", ctx->time())
      ->AddVal("Natural_Uranium", natural_u)
      ->AddVal("SWU", swu)
      ->Record();
}
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double SEnrichment::FeedAssay() {
  using cyclus::Material;

  if (inventory.empty()) {
    return 0;
  }
  double pop_qty = inventory.quantity();
  cyclus::Material::Ptr fission_matl =
      inventory.Pop(pop_qty, cyclus::eps_rsrc());
  inventory.Push(fission_matl);
  return cyclus::toolkit::UraniumAssay(equivalent_u8(fission_matl, ux).first);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
extern "C" cyclus::Agent* ConstructSEnrichment(cyclus::Context* ctx) {
  return new SEnrichment(ctx);
}

}  // namespace cycvt