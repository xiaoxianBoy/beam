#pragma once
#include "../upgradable3/contract.h"
#include "../Math.h"

namespace DaoVault
{
    static const ShaderID s_SID = { 0x65,0xec,0x8c,0xfd,0x2d,0x8a,0xfb,0xc9,0x7c,0xd7,0xad,0x4f,0xdd,0x32,0xe0,0x2e,0x90,0x05,0x5e,0x7a,0x35,0xdb,0x4b,0x5e,0x54,0x3a,0x24,0x3f,0xff,0x6f,0x5f,0x18 };

    typedef MultiPrecision::Float Float;

#pragma pack (push, 1)

    struct Tags
    {
        static const uint8_t s_Pool = 0;
        static const uint8_t s_User = 0;
    };


    struct Pool0
    {
        struct PerAsset
        {
            AssetID m_Aid;
            Amount m_Amount;
            Float m_Sigma;
        };

        Amount m_Weight;
        AssetID m_aidStaking;

        static const uint32_t s_AssetsMax = 128;
    };

    struct PoolMax :public Pool0 {
        PerAsset m_p[s_AssetsMax];
    };

    struct PoolMaxPlus :public PoolMax
    {
        uint32_t m_Assets;

        static Float get_AmountWeighted(Amount amount, Amount weight)
        {
            return Float(amount) / Float(weight);
        }

        void Add(AssetID aid, Amount amount)
        {
            if (!amount)
                return;

            uint32_t i = 0;
            for (; ; i++)
            {
                if (i == m_Assets)
                {
                    Env::Halt_if(i >= s_AssetsMax);
                    auto& x = m_p[i];
                    x.m_Aid = aid;
                    x.m_Amount = 0;
                    x.m_Sigma.Set0();
                    break;
                }

                if (m_p[i].m_Aid == aid)
                    break;
            }

            auto& x = m_p[i];
            Strict::Add(x.m_Amount, amount);

            if (m_Weight)
                x.m_Sigma = x.m_Sigma + get_AmountWeighted(amount, m_Weight);
        }
    };

    struct User0
    {
        struct Key {
            uint8_t m_Tag = Tags::s_User;
            PubKey m_pk;
        };

        struct PerAsset
        {
            Float m_Sigma0;
            Amount m_Value;
        };

        Amount m_Weight;
    };

    struct UserMax :public User0
    {
        PerAsset m_p[Pool0::s_AssetsMax];

        void Remove(PoolMaxPlus& p, uint32_t nAssetsPrev)
        {
            if (!m_Weight)
                return;

            assert(m_Weight <= p.m_Weight);
            Float k = Float(m_Weight);
            p.m_Weight -= m_Weight;

            assert(nAssetsPrev <= p.m_Assets);

            for (uint32_t i = 0; i < p.m_Assets; i++)
            {
                auto& x = p.m_p[i];
                auto& u = m_p[i];
                Amount val = x.m_Amount;

                if (p.m_Weight)
                {
                    auto s = x.m_Sigma;
                    if (i < nAssetsPrev)
                        s = s - u.m_Sigma0;

                    val = std::min<Amount>(val, s * k);
                }
                else
                    x.m_Sigma.Set0(); // time to reset

                x.m_Amount -= val;

                if (i < nAssetsPrev)
                    Strict::Add(u.m_Value, val);
                else
                    u.m_Value = val;
            }
        }

        void Add(PoolMaxPlus& p)
        {
            if (!m_Weight)
                return;

            for (uint32_t i = 0; i < p.m_Assets; i++)
            {
                auto& x = p.m_p[i];
                m_p[i].m_Sigma0 = x.m_Sigma;

                if (!p.m_Weight && x.m_Amount)
                {
                    assert(x.m_Sigma.IsZero());
                    x.m_Sigma = PoolMaxPlus::get_AmountWeighted(x.m_Amount, m_Weight);
                }
            }

            Strict::Add(p.m_Weight, m_Weight);
        }

    };


    namespace Method
    {
        struct Create
        {
            static const uint32_t s_iMethod = 0; // Ctor
            Upgradable3::Settings m_Upgradable;
            AssetID m_aidStaking;
        };

        struct Deposit
        {
            static const uint32_t s_iMethod = 3;
            AssetID m_Aid;
            Amount m_Amount;
        };

        struct UserUpdate
        {
            static const uint32_t s_iMethod = 4;
            PubKey m_pkUser;
            Amount m_NewStaking;
            uint32_t m_WithdrawCount;
            // followed by array of withdraw quantities
        };
    }
#pragma pack (pop)

} // namespace DaoVault