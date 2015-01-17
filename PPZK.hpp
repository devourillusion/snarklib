#ifndef _SNARKLIB_PPZK_HPP_
#define _SNARKLIB_PPZK_HPP_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <istream>
#include <memory>
#include <ostream>
#include <vector>
#include "AuxSTL.hpp"
#include "Group.hpp"
#include "MultiExp.hpp"
#include "Pairing.hpp"
#include "ProgressCallback.hpp"
#include "QAP.hpp"
#include "Rank1DSL.hpp"
#include "WindowExp.hpp"

namespace snarklib {

////////////////////////////////////////////////////////////////////////////////
// Proving key
//

template <typename PAIRING>
class PPZK_ProvingKey
{
    typedef typename PAIRING::G1 G1;
    typedef typename PAIRING::G2 G2;

public:
    PPZK_ProvingKey() = default;

    PPZK_ProvingKey(const SparseVector<Pairing<G1, G1>>& A_query,
                    const SparseVector<Pairing<G2, G1>>& B_query,
                    const SparseVector<Pairing<G1, G1>>& C_query,
                    const std::vector<G1>& H_query,
                    const std::vector<G1>& K_query)
        : m_A_query(A_query),
          m_B_query(B_query),
          m_C_query(C_query),
          m_H_query(H_query),
          m_K_query(K_query)
    {}

    const SparseVector<Pairing<G1, G1>>& A_query() const { return m_A_query; }
    const SparseVector<Pairing<G2, G1>>& B_query() const { return m_B_query; }
    const SparseVector<Pairing<G1, G1>>& C_query() const { return m_C_query; }
    const std::vector<G1>& H_query() const { return m_H_query; }
    const std::vector<G1>& K_query() const { return m_K_query; }

    bool operator== (const PPZK_ProvingKey& other) const {
        return
            A_query() == other.A_query() &&
            B_query() == other.B_query() &&
            C_query() == other.C_query() &&
            H_query() == other.H_query() &&
            K_query() == other.K_query();
    }

    bool operator!= (const PPZK_ProvingKey& other) const {
        return ! (*this == other);
    }

    void marshal_out(std::ostream& os) const {
        A_query().marshal_out(os);
        B_query().marshal_out(os);
        C_query().marshal_out(os);
        snarklib::marshal_out(os, H_query());
        snarklib::marshal_out(os, K_query());
    }

    bool marshal_in(std::istream& is) {
        return
            m_A_query.marshal_in(is) &&
            m_B_query.marshal_in(is) &&
            m_C_query.marshal_in(is) &&
            snarklib::marshal_in(is, m_H_query) &&
            snarklib::marshal_in(is, m_K_query);
    }

    void clear() {
        m_A_query.clear();
        m_B_query.clear();
        m_C_query.clear();
        m_H_query.clear();
        m_K_query.clear();
    }

    bool empty() const {
        return
            m_A_query.empty() ||
            m_B_query.empty() ||
            m_C_query.empty() ||
            m_H_query.empty() ||
            m_K_query.empty();
    }

private:
    SparseVector<Pairing<G1, G1>> m_A_query;
    SparseVector<Pairing<G2, G1>> m_B_query;
    SparseVector<Pairing<G1, G1>> m_C_query;
    std::vector<G1> m_H_query;
    std::vector<G1> m_K_query;
};

////////////////////////////////////////////////////////////////////////////////
// Input consistency
//

template <typename PAIRING>
class PPZK_IC_Query
{
    typedef typename PAIRING::Fr Fr;
    typedef typename PAIRING::G1 G1;

public:
    PPZK_IC_Query() = default;

    PPZK_IC_Query(const G1& base,
                  const std::vector<G1>& encoded_terms)
        : m_base(base),
          m_encoded_terms(encoded_terms)
    {}

    PPZK_IC_Query(const WindowExp<G1>& g1_table,
                  const std::vector<Fr>& coeffs)
        : PPZK_IC_Query{
            coeffs[0] * G1::one(),
            g1_table.batchExp(
                std::vector<Fr>(coeffs.begin() + 1, coeffs.end()))}
    {}

    PPZK_IC_Query accumulate(const R1Witness<Fr>& witness) const
    {
        G1 base = m_base;
        std::vector<G1> encoded_terms;

        const std::size_t
            wsize = witness.size(),
            tsize = m_encoded_terms.size();

        if (wsize < tsize) {
            base = base + multiExp(
                std::vector<G1>(m_encoded_terms.begin(),
                                m_encoded_terms.begin() + wsize),
                *witness);

            encoded_terms
                = std::vector<G1>(m_encoded_terms.begin() + wsize,
                                  m_encoded_terms.end());

        } else if (wsize > tsize) {
            base = base + multiExp(m_encoded_terms,
                                   *witness.truncate(tsize));

        } else {
            base = base + multiExp(m_encoded_terms,
                                   *witness);
        }

        return PPZK_IC_Query(base, encoded_terms);
    }

    const G1& base() const {
        return m_base;
    }

    std::size_t input_size() const {
        return m_encoded_terms.size();
    }

    const std::vector<G1>& encoded_terms() const {
        return m_encoded_terms;
    }

    bool operator== (const PPZK_IC_Query& other) const {
        return
            base() == other.base() &&
            encoded_terms() == other.encoded_terms();
    }

    bool operator!= (const PPZK_IC_Query& other) const {
        return ! (*this == other);
    }

    void marshal_out(std::ostream& os) const {
        base().marshal_out(os);
        snarklib::marshal_out(os, encoded_terms());
    }

    bool marshal_in(std::istream& is) {
        return
            m_base.marshal_in(is) &&
            snarklib::marshal_in(is, m_encoded_terms);
    }

    void clear() {
        m_base = G1::zero();
        m_encoded_terms.clear();
    }

    bool empty() const {
        return
            m_base.isZero() ||
            m_encoded_terms.empty();
    }

private:
    G1 m_base;
    std::vector<G1> m_encoded_terms;
};

////////////////////////////////////////////////////////////////////////////////
// Verification key
//

template <typename PAIRING>
class PPZK_VerificationKey
{
    typedef typename PAIRING::Fr Fr;
    typedef typename PAIRING::G1 G1;
    typedef typename PAIRING::G2 G2;

public:
    PPZK_VerificationKey() = default;

    PPZK_VerificationKey(const G2& alphaA_g2,
                         const G1& alphaB_g1,
                         const G2& alphaC_g2,
                         const G2& gamma_g2,
                         const G1& gamma_beta_g1,
                         const G2& gamma_beta_g2,
                         const G2& rC_Z_g2,
                         const PPZK_IC_Query<PAIRING>& encoded_IC_query)
        : m_alphaA_g2(alphaA_g2),
          m_alphaB_g1(alphaB_g1),
          m_alphaC_g2(alphaC_g2),
          m_gamma_g2(gamma_g2),
          m_gamma_beta_g1(gamma_beta_g1),
          m_gamma_beta_g2(gamma_beta_g2),
          m_rC_Z_g2(rC_Z_g2),
          m_encoded_IC_query(encoded_IC_query)
    {}

    const G2& alphaA_g2() const { return m_alphaA_g2; }
    const G1& alphaB_g1() const { return m_alphaB_g1; }
    const G2& alphaC_g2() const { return m_alphaC_g2; }
    const G2& gamma_g2() const { return m_gamma_g2; }
    const G1& gamma_beta_g1() const { return m_gamma_beta_g1; }
    const G2& gamma_beta_g2() const { return m_gamma_beta_g2; }
    const G2& rC_Z_g2() const { return m_rC_Z_g2; }

    const PPZK_IC_Query<PAIRING>& encoded_IC_query() const {
        return m_encoded_IC_query;
    }

    bool operator== (const PPZK_VerificationKey& other) const {
        return
            alphaA_g2() == other.alphaA_g2() &&
            alphaB_g1() == other.alphaB_g1() &&
            alphaC_g2() == other.alphaC_g2() &&
            gamma_g2() == other.gamma_g2() &&
            gamma_beta_g1() == other.gamma_beta_g1() &&
            gamma_beta_g2() == other.gamma_beta_g2() &&
            rC_Z_g2() == other.rC_Z_g2() &&
            encoded_IC_query() == other.encoded_IC_query();
    }

    bool operator!= (const PPZK_VerificationKey& other) const {
        return ! (*this == other);
    }

    void marshal_out(std::ostream& os) const {
        alphaA_g2().marshal_out(os);
        alphaB_g1().marshal_out(os);
        alphaC_g2().marshal_out(os);
        gamma_g2().marshal_out(os);
        gamma_beta_g1().marshal_out(os);
        gamma_beta_g2().marshal_out(os);
        rC_Z_g2().marshal_out(os);
        encoded_IC_query().marshal_out(os);
    }

    bool marshal_in(std::istream& is) {
        return
            m_alphaA_g2.marshal_in(is) &&
            m_alphaB_g1.marshal_in(is) &&
            m_alphaC_g2.marshal_in(is) &&
            m_gamma_g2.marshal_in(is) &&
            m_gamma_beta_g1.marshal_in(is) &&
            m_gamma_beta_g2.marshal_in(is) &&
            m_rC_Z_g2.marshal_in(is) &&
            m_encoded_IC_query.marshal_in(is);
    }

    void clear() {
        m_alphaA_g2 = G2::zero();
        m_alphaB_g1 = G1::zero();
        m_alphaC_g2 = G2::zero();
        m_gamma_g2 = G2::zero();
        m_gamma_beta_g1 = G1::zero();
        m_gamma_beta_g2 = G2::zero();
        m_rC_Z_g2 = G2::zero();
        m_encoded_IC_query.clear();
    }

    bool empty() const {
        return
            m_alphaA_g2.isZero() ||
            m_alphaB_g1.isZero() ||
            m_alphaC_g2.isZero() ||
            m_gamma_g2.isZero() ||
            m_gamma_beta_g1.isZero() ||
            m_gamma_beta_g2.isZero() ||
            m_rC_Z_g2.isZero() ||
            m_encoded_IC_query.empty();
    }

private:
    G2 m_alphaA_g2;
    G1 m_alphaB_g1;
    G2 m_alphaC_g2;
    G2 m_gamma_g2;
    G1 m_gamma_beta_g1;
    G2 m_gamma_beta_g2;
    G2 m_rC_Z_g2;
    PPZK_IC_Query<PAIRING> m_encoded_IC_query;
};

////////////////////////////////////////////////////////////////////////////////
// Precomputed verification key (Miller loop input)
//

template <typename PAIRING>
class PPZK_PrecompVerificationKey
{
    typedef typename PAIRING::G2 G2;
    typedef typename PAIRING::G1_precomp G1_precomp;
    typedef typename PAIRING::G2_precomp G2_precomp;

public:
    PPZK_PrecompVerificationKey(const PPZK_VerificationKey<PAIRING>& vk)
        : m_pp_G2_one_precomp(G2::one()),
          m_vk_alphaA_g2_precomp(vk.alphaA_g2()),
          m_vk_alphaB_g1_precomp(vk.alphaB_g1()),
          m_vk_alphaC_g2_precomp(vk.alphaC_g2()),
          m_vk_rC_Z_g2_precomp(vk.rC_Z_g2()),
          m_vk_gamma_g2_precomp(vk.gamma_g2()),
          m_vk_gamma_beta_g1_precomp(vk.gamma_beta_g1()),
          m_vk_gamma_beta_g2_precomp(vk.gamma_beta_g2()),
          m_encoded_IC_query(vk.encoded_IC_query())
    {}

    const G2_precomp& pp_G2_one_precomp() const { return m_pp_G2_one_precomp; }
    const G2_precomp& vk_alphaA_g2_precomp() const { return m_vk_alphaA_g2_precomp; }
    const G1_precomp& vk_alphaB_g1_precomp() const { return m_vk_alphaB_g1_precomp; }
    const G2_precomp& vk_alphaC_g2_precomp() const { return m_vk_alphaC_g2_precomp; }
    const G2_precomp& vk_rC_Z_g2_precomp() const { return m_vk_rC_Z_g2_precomp; }
    const G2_precomp& vk_gamma_g2_precomp() const { return m_vk_gamma_g2_precomp; }
    const G1_precomp& vk_gamma_beta_g1_precomp() const { return m_vk_gamma_beta_g1_precomp; }
    const G2_precomp& vk_gamma_beta_g2_precomp() const { return m_vk_gamma_beta_g2_precomp; }

    const PPZK_IC_Query<PAIRING>& encoded_IC_query() const {
        return m_encoded_IC_query;
    }

private:
    G2_precomp m_pp_G2_one_precomp;
    G2_precomp m_vk_alphaA_g2_precomp;
    G1_precomp m_vk_alphaB_g1_precomp;
    G2_precomp m_vk_alphaC_g2_precomp;
    G2_precomp m_vk_rC_Z_g2_precomp;
    G2_precomp m_vk_gamma_g2_precomp;
    G1_precomp m_vk_gamma_beta_g1_precomp;
    G2_precomp m_vk_gamma_beta_g2_precomp;
    PPZK_IC_Query<PAIRING> m_encoded_IC_query;
};

////////////////////////////////////////////////////////////////////////////////
// Key pair: proving and verification
//

template <typename PAIRING>
class PPZK_Keypair
{
    typedef typename PAIRING::Fr Fr;
    typedef typename PAIRING::G1 G1;
    typedef typename PAIRING::G2 G2;

public:
    PPZK_Keypair() = default;

    PPZK_Keypair(const PPZK_ProvingKey<PAIRING>& pk,
                 const PPZK_VerificationKey<PAIRING> vk)
        : m_pk(pk),
          m_vk(vk)
    {}

    PPZK_Keypair(const R1System<Fr>& constraintSystem,
                 const std::size_t numCircuitInputs,
                 ProgressCallback* callback = nullptr)
    {
        ProgressCallback_NOP<PAIRING> dummyNOP;
        ProgressCallback* dummy = callback ? callback : std::addressof(dummyNOP);

        dummy->majorSteps(7); // 7 major steps

        // randomness
        const auto
            point = Fr::random(),
            alphaA = Fr::random(),
            alphaB = Fr::random(),
            alphaC = Fr::random(),
            rA = Fr::random(),
            rB = Fr::random(),
            beta = Fr::random(),
            gamma = Fr::random();

        const auto rC = rA * rB;

        const QAP_SystemPoint<Fr> qap(constraintSystem, numCircuitInputs, point);

        // ABCH
        QAP_QueryA<Fr> At(qap); // changed by IC_coefficients
        const QAP_QueryB<Fr> Bt(qap);
        const QAP_QueryC<Fr> Ct(qap);
        const QAP_QueryH<Fr> Ht(qap);

        dummy->major(true); // step 7 (starting)

        const WindowExp<G1> g1_table(g1_exp_count(qap, At, Bt, Ct, Ht), callback);

        dummy->major(true); // step 6

        const WindowExp<G2> g2_table(g2_exp_count(Bt), callback);

        dummy->major(true); // step 5

        auto Kt = g1_table.batchExp(QAP_QueryK<Fr>(qap, At, Bt, Ct, rA, rB, beta).vec(), callback);
#ifdef USE_ADD_SPECIAL
        batchSpecial(Kt);
#endif

        // side-effect: this modifies At query vector
        const QAP_IC_coefficients<Fr> IC_coefficients(qap, At, rA);

        m_pk = PPZK_ProvingKey<PAIRING>(
            (dummy->major(true), // step 4
             batchExp(g1_table, g1_table, rA, rA * alphaA, At.vec(), callback)),

            (dummy->major(true), // step 3
             batchExp(g2_table, g1_table, rB, rB * alphaB, Bt.vec(), callback)),

            (dummy->major(true), // step 2
             batchExp(g1_table, g1_table, rC, rC * alphaC, Ct.vec(), callback)),

            (dummy->major(true), // step 1
             g1_table.batchExp(Ht.vec(), callback)),

            Kt);

        m_vk = PPZK_VerificationKey<PAIRING>(
            alphaA * G2::one(),
            alphaB * G1::one(),
            alphaC * G2::one(),
            gamma * G2::one(),
            (gamma * beta) * G1::one(),
            (gamma * beta) * G2::one(),
            (rC * qap.compute_Z()) * G2::one(),
            PPZK_IC_Query<PAIRING>(g1_table, IC_coefficients.vec()));
    }

    const PPZK_ProvingKey<PAIRING>& pk() const { return m_pk; }
    const PPZK_VerificationKey<PAIRING>& vk() const { return m_vk; }

    bool operator== (const PPZK_Keypair& other) const {
        return
            pk() == other.pk() &&
            vk() == other.vk();
    }

    bool operator!= (const PPZK_Keypair& other) const {
        return ! (*this == other);
    }

    void marshal_out(std::ostream& os) const {
        pk().marshal_out(os);
        vk().marshal_out(os);
    }

    bool marshal_in(std::istream& is) {
        return
            m_pk.marshal_in(is) &&
            m_vk.marshal_in(is);
    }

    void clear() {
        m_pk.clear();
        m_vk.clear();
    }

    bool empty() const {
        return
            m_pk.empty() ||
            m_vk.empty();
    }

private:
    PPZK_ProvingKey<PAIRING> m_pk;
    PPZK_VerificationKey<PAIRING> m_vk;
};

template <typename PAIRING>
std::ostream& operator<< (std::ostream& os, const PPZK_Keypair<PAIRING>& a) {
    a.marshal_out(os);
    return os;
}

template <typename PAIRING>
std::istream& operator>> (std::istream& is, PPZK_Keypair<PAIRING>& a) {
    if (! a.marshal_in(is)) a.clear();
    return is;
}

////////////////////////////////////////////////////////////////////////////////
// Proof
//

template <typename PAIRING>
class PPZK_Proof
{
    typedef typename PAIRING::Fr Fr;
    typedef typename PAIRING::G1 G1;
    typedef typename PAIRING::G2 G2;

public:
    PPZK_Proof() = default;

    PPZK_Proof(const Pairing<G1, G1>& A,
               const Pairing<G2, G1>& B,
               const Pairing<G1, G1>& C,
               const G1& H,
               const G1& K)
        : m_A(A),
          m_B(B),
          m_C(C),
          m_H(H),
          m_K(K)
    {}

    PPZK_Proof(const R1System<Fr>& constraintSystem,
               const std::size_t numCircuitInputs,
               const PPZK_ProvingKey<PAIRING>& pk,
               const R1Witness<Fr>& witness,
               const std::size_t reserveTune,
               ProgressCallback* callback)
    {
        ProgressCallback_NOP<PAIRING> dummyNOP;
        ProgressCallback* dummy = callback ? callback : std::addressof(dummyNOP);

        dummy->majorSteps(5); // 5 major steps

        // randomness
        const auto
            d1 = Fr::random(),
            d2 = Fr::random(),
            d3 = Fr::random();

        const QAP_SystemPoint<Fr> qap(constraintSystem, numCircuitInputs);

        // ABCH
        QAP_WitnessA<Fr> aA(qap, witness);
        QAP_WitnessB<Fr> aB(qap, witness);
        QAP_WitnessC<Fr> aC(qap, witness);
        QAP_WitnessH<Fr> aH(qap, aA, aB, d1, d2, d3);

        aA.cosetFFT();
        aB.cosetFFT();
        aC.cosetFFT();

        aH.addTemporary(QAP_WitnessH<Fr>(qap, aA, aB, aC));

        dummy->major(true); // step 5 (starting)

        const auto& A_query = pk.A_query();
        const auto& B_query = pk.B_query();
        const auto& C_query = pk.C_query();
        const auto& H_query = pk.H_query();
        const auto& K_query = pk.K_query();

        // A
        m_A = (d1 * A_query.getElementForIndex(0)) + A_query.getElementForIndex(3);
        if (0 == reserveTune) {
            m_A = m_A + multiExp01(A_query,
                                   *witness,
                                   4,
                                   4 + qap.numVariables(),
                                   callback);
        } else {
            m_A = m_A + multiExp01(A_query,
                                   *witness,
                                   4,
                                   4 + qap.numVariables(),
                                   qap.numVariables() / reserveTune,
                                   callback);
        }

        dummy->major(true); // step 4

        // B
        m_B = (d2 * B_query.getElementForIndex(1)) + B_query.getElementForIndex(3);
        if (0 == reserveTune) {
            m_B = m_B + multiExp01(B_query,
                                   *witness,
                                   4,
                                   4 + qap.numVariables(),
                                   callback);
        } else {
            m_B = m_B + multiExp01(B_query,
                                   *witness,
                                   4,
                                   4 + qap.numVariables(),
                                   qap.numVariables() / reserveTune,
                                   callback);
        }

        dummy->major(true); // step 3

        // C
        m_C = (d3 * C_query.getElementForIndex(2)) + C_query.getElementForIndex(3);
        if (0 == reserveTune) {
            m_C = m_C + multiExp01(C_query,
                                   *witness,
                                   4,
                                   4 + qap.numVariables(),
                                   callback);
        } else {
            m_C = m_C + multiExp01(C_query,
                                   *witness,
                                   4,
                                   4 + qap.numVariables(),
                                   qap.numVariables() / reserveTune,
                                   callback);
        }

        dummy->major(true); // step 2

        // H
        m_H = multiExp(H_query, aH.vec(), callback);

        dummy->major(true); // step 1

        // K
        m_K = (d1 * K_query[0]) + (d2 * K_query[1]) + (d3 * K_query[2]) + K_query[3];
        if (0 == reserveTune) {
            m_K = m_K + multiExp01(std::vector<G1>(K_query.begin() + 4, K_query.end()),
                                   *witness,
                                   callback);
        } else {
            m_K = m_K + multiExp01(std::vector<G1>(K_query.begin() + 4, K_query.end()),
                                   *witness,
                                   (K_query.size() - 4) / reserveTune,
                                   callback);
        }
    }

    PPZK_Proof(const R1System<Fr>& constraintSystem,
               const std::size_t numCircuitInputs,
               const PPZK_ProvingKey<PAIRING>& pk,
               const R1Witness<Fr>& witness,
               ProgressCallback* callback = nullptr)
        : PPZK_Proof{constraintSystem, numCircuitInputs, pk, witness, 0, callback}
    {}

    const Pairing<G1, G1>& A() const { return m_A; }
    const Pairing<G2, G1>& B() const { return m_B; }
    const Pairing<G1, G1>& C() const { return m_C; }
    const G1& H() const { return m_H; }
    const G1& K() const { return m_K; }

    bool wellFormed() const {
        return
            m_A.G().wellFormed() && m_A.H().wellFormed() &&
            m_B.G().wellFormed() && m_B.H().wellFormed() &&
            m_C.G().wellFormed() && m_C.H().wellFormed() &&
            m_H.wellFormed() &&
            m_K.wellFormed();
    }

    bool operator== (const PPZK_Proof& other) const {
        return
            A() == other.A() &&
            B() == other.B() &&
            C() == other.C() &&
            H() == other.H() &&
            K() == other.K();
    }

    bool operator!= (const PPZK_Proof& other) const {
        return ! (*this == other);
    }

    void marshal_out(std::ostream& os) const {
        A().marshal_out(os);
        B().marshal_out(os);
        C().marshal_out(os);
        H().marshal_out(os);
        K().marshal_out(os);
    }

    bool marshal_in(std::istream& is) {
        return
            m_A.marshal_in(is) &&
            m_B.marshal_in(is) &&
            m_C.marshal_in(is) &&
            m_H.marshal_in(is) &&
            m_K.marshal_in(is);
    }

    void clear() {
        m_A = Pairing<G1, G1>::zero();
        m_B = Pairing<G2, G1>::zero();
        m_C = Pairing<G1, G1>::zero();
        m_H = G1::zero();
        m_K = G1::zero();
    }

    bool empty() const {
        return
            m_A.isZero() ||
            m_B.isZero() ||
            m_C.isZero() ||
            m_H.isZero() ||
            m_K.isZero();
    }

private:
    Pairing<G1, G1> m_A;
    Pairing<G2, G1> m_B;
    Pairing<G1, G1> m_C;
    G1 m_H;
    G1 m_K;
};

template <typename PAIRING>
std::ostream& operator<< (std::ostream& os, const PPZK_Proof<PAIRING>& a) {
    a.marshal_out(os);
    return os;
}

template <typename PAIRING>
std::istream& operator>> (std::istream& is, PPZK_Proof<PAIRING>& a) {
    if (! a.marshal_in(is)) a.clear();
    return is;
}

////////////////////////////////////////////////////////////////////////////////
// Verification functions
//

template <typename PAIRING>
bool weakVerify(const PPZK_PrecompVerificationKey<PAIRING>& pvk,
                const R1Witness<typename PAIRING::Fr>& input,
                const PPZK_Proof<PAIRING>& proof,
                ProgressCallback* callback = nullptr)
{
    ProgressCallback_NOP<PAIRING> dummyNOP;
    ProgressCallback* dummy = callback ? callback : std::addressof(dummyNOP);

    dummy->majorSteps(5); // 5 major steps

    dummy->major(); // step 5 (starting)

    typedef typename PAIRING::GT GT;
    typedef typename PAIRING::G1_precomp G1_precomp;
    typedef typename PAIRING::G2_precomp G2_precomp;

    const auto accum_IC = pvk.encoded_IC_query().accumulate(input);
    assert(0 == accum_IC.input_size());

    if (! proof.wellFormed()) {
        return false;
    }

    const auto ONE = GT::one();

     // knowledge commitment for A
    const G1_precomp proof_g_A_g_precomp(proof.A().G());
    const G1_precomp proof_g_A_h_precomp(proof.A().H());
    const auto kc_A_1 = PAIRING::ate_miller_loop(proof_g_A_g_precomp, pvk.vk_alphaA_g2_precomp());
    const auto kc_A_2 = PAIRING::ate_miller_loop(proof_g_A_h_precomp, pvk.pp_G2_one_precomp());
    const auto kc_A = PAIRING::final_exponentiation(kc_A_1 * unitary_inverse(kc_A_2));
    if (ONE != kc_A) {
        return false;
    }

    dummy->major(); // step 4

    // knowledge commitment for B
    const G2_precomp proof_g_B_g_precomp(proof.B().G());
    const G1_precomp proof_g_B_h_precomp(proof.B().H());
    const auto kc_B_1 = PAIRING::ate_miller_loop(pvk.vk_alphaB_g1_precomp(), proof_g_B_g_precomp);
    const auto kc_B_2 = PAIRING::ate_miller_loop(proof_g_B_h_precomp, pvk.pp_G2_one_precomp());
    const auto kc_B = PAIRING::final_exponentiation(kc_B_1 * unitary_inverse(kc_B_2));
    if (ONE != kc_B) {
        return false;
    }

    dummy->major(); // step 3

    // knowledge commitment for C
    const G1_precomp proof_g_C_g_precomp(proof.C().G());
    const G1_precomp proof_g_C_h_precomp(proof.C().H());
    const auto kc_C_1 = PAIRING::ate_miller_loop(proof_g_C_g_precomp, pvk.vk_alphaC_g2_precomp());
    const auto kc_C_2 = PAIRING::ate_miller_loop(proof_g_C_h_precomp, pvk.pp_G2_one_precomp());
    const auto kc_C = PAIRING::final_exponentiation(kc_C_1 * unitary_inverse(kc_C_2));
    if (ONE != kc_C) {
        return false;
    }

    dummy->major(); // step 2

    // quadratic arithmetic program divisibility
    const G1_precomp proof_g_A_g_acc_precomp(proof.A().G() + accum_IC.base());
    const G1_precomp proof_g_H_precomp(proof.H());
    const auto QAP_1 = PAIRING::ate_miller_loop(proof_g_A_g_acc_precomp, proof_g_B_g_precomp);
    const auto QAP_23 = PAIRING::ate_double_miller_loop(proof_g_H_precomp, pvk.vk_rC_Z_g2_precomp(), proof_g_C_g_precomp, pvk.pp_G2_one_precomp());
    const auto QAP = PAIRING::final_exponentiation(QAP_1 * unitary_inverse(QAP_23));
    if (ONE != QAP) {
        return false;
    }

    dummy->major(); // step 1

    // same coefficients
    const G1_precomp proof_g_K_precomp(proof.K());
    const G1_precomp proof_g_A_g_acc_C_precomp(proof.A().G() + accum_IC.base() + proof.C().G());
    const auto K_1 = PAIRING::ate_miller_loop(proof_g_K_precomp, pvk.vk_gamma_g2_precomp());
    const auto K_23 = PAIRING::ate_double_miller_loop(proof_g_A_g_acc_C_precomp, pvk.vk_gamma_beta_g2_precomp(), pvk.vk_gamma_beta_g1_precomp(), proof_g_B_g_precomp);
    const auto K = PAIRING::final_exponentiation(K_1 * unitary_inverse(K_23));
    if (ONE != K) {
        return false;
    }

    return true;
}

template <typename PAIRING>
bool weakVerify(const PPZK_VerificationKey<PAIRING>& vk,
                const R1Witness<typename PAIRING::Fr>& input,
                const PPZK_Proof<PAIRING>& proof,
                ProgressCallback* callback = nullptr)
{
    return weakVerify(PPZK_PrecompVerificationKey<PAIRING>(vk),
                      input,
                      proof,
                      callback);
}

template <typename PAIRING>
bool strongVerify(const PPZK_PrecompVerificationKey<PAIRING>& pvk,
                  const R1Witness<typename PAIRING::Fr>& input,
                  const PPZK_Proof<PAIRING>& proof,
                  ProgressCallback* callback = nullptr)
{
    return (pvk.encoded_IC_query().input_size() == input.size())
        ? weakVerify(pvk, input, proof, callback)
        : false;
}

template <typename PAIRING>
bool strongVerify(const PPZK_VerificationKey<PAIRING>& vk,
                  const R1Witness<typename PAIRING::Fr>& input,
                  const PPZK_Proof<PAIRING>& proof,
                  ProgressCallback* callback = nullptr)
{
    return strongVerify(PPZK_PrecompVerificationKey<PAIRING>(vk),
                        input,
                        proof,
                        callback);
}

} // namespace snarklib

#endif
