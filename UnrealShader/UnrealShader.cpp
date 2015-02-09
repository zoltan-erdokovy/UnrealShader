#include <lx_shade.hpp>
#include <lx_vector.hpp>
#include <lx_package.hpp>
#include <lx_action.hpp>
#include <lx_value.hpp>
#include <lx_log.hpp>
#include <lx_channelui.hpp>
#include <lx_item.hpp>
#include <lxcommand.h>
#include <lxidef.h>
#include <lx_raycast.hpp>
#include <lx_shdr.hpp>
#include <lx_tableau.hpp>

#include <math.h>
#include <string>


namespace Unreal_Shader {	// disambiguate everything with a namespace


	/*
	* Vector Packet definition: Unreal Packet
	* Here we define the packet that will be used to carry the shading parameters through the shading pipe
	*/
	class UnrealPacket : public CLxImpl_VectorPacket
	{
	public:
		UnrealPacket() {}

		static LXtTagInfoDesc	descInfo[];

		unsigned int	vpkt_Size() LXx_OVERRIDE;
		const LXtGUID * vpkt_Interface(void) LXx_OVERRIDE;
		LxResult	vpkt_Initialize(void	*packet) LXx_OVERRIDE;
		LxResult	vpkt_Blend(void	*packet, void	*p0, void	*p1, float	t, int	mode) LXx_OVERRIDE;
	};

#define SRVs_UNREAL_VPACKET		"unreal.shader.packet"
#define LXsP_SAMPLE_UNREAL		SRVs_UNREAL_VPACKET

	LXtTagInfoDesc	 UnrealPacket::descInfo[] = {
			{ LXsSRV_USERNAME, "Unreal Shader Packet" },
			{ LXsSRV_LOGSUBSYSTEM, "vector-packet" },
			{ LXsVPK_CATEGORY, LXsCATEGORY_SAMPLE },
			{ 0 }
	};

	typedef struct st_LXpUnreal
	{
		LXtFVector	baseColor;
		float metallic;
		float specular;
		float roughness;
		LXtFVector	emissiveColor;
		float uOpacity;
		float tessMultiplier;
		LXtFVector	auxRGB;
	} LXpUnreal;


	unsigned int UnrealPacket::vpkt_Size(void)
	{
		return	sizeof(LXpUnreal);
	}

	const LXtGUID * UnrealPacket::vpkt_Interface(void)
	{
		return NULL;
	}

	LxResult UnrealPacket::vpkt_Initialize(void	*p)
	{
		LXpUnreal		*csp = (LXpUnreal *)p;

		LXx_VCLR(csp->baseColor);
		return LXe_OK;
	}

	LxResult UnrealPacket::vpkt_Blend(void *p, void *p0, void *p1, float t, int mode)
	{
		LXpUnreal *sp = (LXpUnreal *)p;
		LXpUnreal *sp0 = (LXpUnreal *)p0;
		LXpUnreal *sp1 = (LXpUnreal *)p1;
		CLxLoc_ShaderService	 shdrSrv;

		shdrSrv.ColorBlendValue(sp->baseColor, sp0->baseColor, sp1->baseColor, t, mode);
		sp->metallic = shdrSrv.ScalarBlendValue(sp0->metallic, sp1->metallic, t, mode);
		sp->specular = shdrSrv.ScalarBlendValue(sp0->specular, sp1->specular, t, mode);
		sp->roughness = shdrSrv.ScalarBlendValue(sp0->roughness, sp1->roughness, t, mode);
		shdrSrv.ColorBlendValue(sp->emissiveColor, sp0->emissiveColor, sp1->emissiveColor, t, mode);
		sp->uOpacity = shdrSrv.ScalarBlendValue(sp0->uOpacity, sp1->uOpacity, t, mode);
		sp->tessMultiplier = shdrSrv.ScalarBlendValue(sp0->tessMultiplier, sp1->tessMultiplier, t, mode);
		shdrSrv.ColorBlendValue(sp->auxRGB, sp0->auxRGB, sp1->auxRGB, t, mode);

		return LXe_OK;
	}


	/*
	* Physically based Material
	*/
	class UnrealMaterial : public CLxImpl_CustomMaterial, public CLxImpl_ChannelUI
	{
	public:
		UnrealMaterial() {}

		static LXtTagInfoDesc	descInfo[];

		int				cmt_Flags() LXx_OVERRIDE;
		LxResult		cmt_SetupChannels(ILxUnknownID addChan) LXx_OVERRIDE;
		LxResult		cmt_LinkChannels(ILxUnknownID eval, ILxUnknownID item) LXx_OVERRIDE;
		LxResult		cmt_LinkSampleChannels(ILxUnknownID eval, ILxUnknownID item, int *idx) LXx_OVERRIDE;
		LxResult		cmt_ReadChannels(ILxUnknownID attr, void **ppvData) LXx_OVERRIDE;
		LxResult		cmt_CustomPacket(const char	**) LXx_OVERRIDE;
		void			cmt_MaterialEvaluate(ILxUnknownID etor, int *idx, ILxUnknownID vector, void *data) LXx_OVERRIDE;
		void			cmt_ShaderEvaluate(ILxUnknownID vector, ILxUnknownID rayObj, LXpShadeComponents	*sCmp, LXpShadeOutput *sOut, void*data) LXx_OVERRIDE;
		void			cmt_Cleanup(void *data) LXx_OVERRIDE;

		LxResult		cmt_SetOpaque(int *opaque) LXx_OVERRIDE;

		LxResult		cui_Enabled(const char *channelName, ILxUnknownID msg, ILxUnknownID item, ILxUnknownID read);
		LxResult		cui_UIHints(const char *channelName, ILxUnknownID hints) LXx_OVERRIDE;

		LXtItemType		MyType();

		CLxUser_PacketService	pkt_service;
		CLxUser_NodalService	nodalSvc;

		unsigned		nrm_offset;
		unsigned		tex_offset;
		unsigned		prm_offset;
		unsigned		drv_offset;
		unsigned		msk_offset;
		unsigned		dis_offset;
		unsigned		pkt_offset;

		LXtItemType		my_type;

		LXtSampleIndex		idxs[17];     // indices to each data channel in RendData

		class RendData
		{
		public:
			bool		shaderMode;
			bool		bakingMode;
			LXtFVector	baseColor;
			float metallic;
			float specular;
			float roughness;
			LXtFVector	emissiveColor;
			float uOpacity;
			float tessMultiplier;
			int channelDebug;
			LXtFVector	auxRGB;
		};

	private:
		int channelDebugMode;
	};

#define SRVs_UNREAL_MATR			"unreal"
#define SRVs_UNREAL_MATR_ITEMTYPE	"material." SRVs_UNREAL_MATR

	LXtTagInfoDesc UnrealMaterial::descInfo[] =
	{
			{ LXsSRV_USERNAME, "Unreal Shader" },
			{ LXsSRV_LOGSUBSYSTEM, "comp-shader" },
			{ 0 }
	};

	/*
	* clean up render data
	*/
	void UnrealMaterial::cmt_Cleanup(void *data)
	{
		RendData* rd = (RendData*)data;
		delete rd;
	}

/*
* Setup channels for the item type.
*/
	enum {
		DEBUG_OFF = 0,
		DEBUG_BASECOLOR,
		DEBUG_METALLIC,
		DEBUG_SPECULAR,
		DEBUG_ROUGHNESS,
		DEBUG_EMISSIVECOLOR,
		DEBUG_OPACITY,
		DEBUG_TESSMULTIPLIER,
		DEBUG_AUXRGB,
		DEBUG_END
	};

	static LXtTextValueHint hint_ChannelDebug[] = {
		DEBUG_OFF, "off",
		DEBUG_BASECOLOR, "baseColor",
		DEBUG_METALLIC, "metallic",
		DEBUG_SPECULAR, "specular",
		DEBUG_ROUGHNESS, "roughness",
		DEBUG_EMISSIVECOLOR, "emissiveColor",
		DEBUG_OPACITY, "opacity",
		DEBUG_TESSMULTIPLIER, "tessMultiplier",
		DEBUG_AUXRGB, "auxRGB",
		-1, "=channel-debug",
		-1, 0
	};

#define UNREAL_CH_SHADERMODE		"shaderMode"
#define UNREAL_CH_BAKINGMODE		"bakingMode"
#define UNREAL_CH_BASECOLOR			"baseColor"
#define UNREAL_CH_METALLIC			"metallic"
#define UNREAL_CH_SPECULAR			"specular"
#define UNREAL_CH_ROUGHNESS			"roughness"
#define UNREAL_CH_EMISSIVECOLOR		"emissiveColor"
#define UNREAL_CH_OPACITY			"uOpacity"
#define UNREAL_CH_TESSMULTIPLIER	"tessMultiplier"
#define UNREAL_CH_CHANNELDEBUG		"channelDebug"
#define UNREAL_CH_AUXRGB			"auxRGB"

	int UnrealMaterial::cmt_Flags()
	{
		return	0;
	}

	LxResult UnrealMaterial::cmt_SetupChannels(ILxUnknownID addChan)
	{
		CLxUser_AddChannel ac(addChan);
		LXtVector zero = { 0, 0, 0 };
		LXtVector one = { 1, 1, 1 };

		ac.NewChannel(UNREAL_CH_SHADERMODE, LXsTYPE_BOOLEAN);
		ac.SetDefault(0.0, 0);

		ac.NewChannel(UNREAL_CH_BAKINGMODE, LXsTYPE_BOOLEAN);
		ac.SetDefault(0.0, 0);

		ac.NewChannel(UNREAL_CH_BASECOLOR, LXsTYPE_COLOR1);
		ac.SetVector(LXsCHANVEC_RGB);
		ac.SetDefaultVec(one);

		ac.NewChannel(UNREAL_CH_METALLIC, LXsTYPE_FLOAT);
		ac.SetDefault(0.0, 0);

		ac.NewChannel(UNREAL_CH_SPECULAR, LXsTYPE_FLOAT);
		ac.SetDefault(0.5, 0);

		ac.NewChannel(UNREAL_CH_ROUGHNESS, LXsTYPE_FLOAT);
		ac.SetDefault(0.5, 0);

		ac.NewChannel(UNREAL_CH_EMISSIVECOLOR, LXsTYPE_COLOR1);
		ac.SetVector(LXsCHANVEC_RGB);
		ac.SetDefaultVec(zero);

		ac.NewChannel(UNREAL_CH_OPACITY, LXsTYPE_FLOAT);
		ac.SetDefault(1.0, 0);

		ac.NewChannel(UNREAL_CH_TESSMULTIPLIER, LXsTYPE_FLOAT);
		ac.SetDefault(0.5, 0);

		ac.NewChannel(UNREAL_CH_CHANNELDEBUG, LXsTYPE_INTEGER);
		ac.SetDefault(0.0, DEBUG_OFF);
		ac.SetHint(hint_ChannelDebug);

		ac.NewChannel(UNREAL_CH_AUXRGB, LXsTYPE_COLOR1);
		ac.SetVector(LXsCHANVEC_RGB);
		ac.SetDefaultVec(zero);

		return LXe_OK;
	}

	/*
	* Attach to channel evaluations. This gets the indicies for the channels in attributes.
	*/
	LxResult UnrealMaterial::cmt_LinkChannels(ILxUnknownID eval, ILxUnknownID item)
	{
		CLxUser_Evaluation	 ev(eval);
		CLxUser_Item		 it(item);

		idxs[0].chan = it.ChannelIndex(UNREAL_CH_SHADERMODE);
		idxs[1].chan = it.ChannelIndex(UNREAL_CH_BAKINGMODE);

		idxs[2].chan = it.ChannelIndex(UNREAL_CH_BASECOLOR".R");
		idxs[3].chan = it.ChannelIndex(UNREAL_CH_BASECOLOR".G");
		idxs[4].chan = it.ChannelIndex(UNREAL_CH_BASECOLOR".B");

		idxs[5].chan = it.ChannelIndex(UNREAL_CH_METALLIC);
		idxs[6].chan = it.ChannelIndex(UNREAL_CH_SPECULAR);
		idxs[7].chan = it.ChannelIndex(UNREAL_CH_ROUGHNESS);

		idxs[8].chan = it.ChannelIndex(UNREAL_CH_EMISSIVECOLOR".R");
		idxs[9].chan = it.ChannelIndex(UNREAL_CH_EMISSIVECOLOR".G");
		idxs[10].chan = it.ChannelIndex(UNREAL_CH_EMISSIVECOLOR".B");

		idxs[11].chan = it.ChannelIndex(UNREAL_CH_OPACITY);
		idxs[12].chan = it.ChannelIndex(UNREAL_CH_TESSMULTIPLIER);

		idxs[13].chan = it.ChannelIndex(UNREAL_CH_CHANNELDEBUG);

		idxs[14].chan = it.ChannelIndex(UNREAL_CH_AUXRGB".R");
		idxs[15].chan = it.ChannelIndex(UNREAL_CH_AUXRGB".G");
		idxs[16].chan = it.ChannelIndex(UNREAL_CH_AUXRGB".B");

		idxs[0].layer = ev.AddChan(item, idxs[0].chan);
		idxs[1].layer = ev.AddChan(item, idxs[1].chan);

		idxs[2].layer = ev.AddChan(item, idxs[2].chan);
		idxs[3].layer = ev.AddChan(item, idxs[3].chan);
		idxs[4].layer = ev.AddChan(item, idxs[4].chan);

		idxs[5].layer = ev.AddChan(item, idxs[5].chan);
		idxs[6].layer = ev.AddChan(item, idxs[6].chan);
		idxs[7].layer = ev.AddChan(item, idxs[7].chan);

		idxs[8].layer = ev.AddChan(item, idxs[8].chan);
		idxs[9].layer = ev.AddChan(item, idxs[9].chan);
		idxs[10].layer = ev.AddChan(item, idxs[10].chan);

		idxs[11].layer = ev.AddChan(item, idxs[11].chan);
		idxs[12].layer = ev.AddChan(item, idxs[12].chan);

		idxs[13].layer = ev.AddChan(item, idxs[13].chan);

		idxs[14].layer = ev.AddChan(item, idxs[14].chan);
		idxs[15].layer = ev.AddChan(item, idxs[15].chan);
		idxs[16].layer = ev.AddChan(item, idxs[16].chan);

		nrm_offset = pkt_service.GetOffset(LXsCATEGORY_SAMPLE, LXsP_SURF_NORMAL);
		tex_offset = pkt_service.GetOffset(LXsCATEGORY_SAMPLE, LXsP_TEXTURE_INPUT);
		prm_offset = pkt_service.GetOffset(LXsCATEGORY_SAMPLE, LXsP_SAMPLE_PARMS);
		drv_offset = pkt_service.GetOffset(LXsCATEGORY_SAMPLE, LXsP_SAMPLE_DRIVER);
		msk_offset = pkt_service.GetOffset(LXsCATEGORY_SAMPLE, LXsP_SAMPLE_MASK);
		dis_offset = pkt_service.GetOffset(LXsCATEGORY_SAMPLE, LXsP_DISPLACE);
		pkt_offset = pkt_service.GetOffset(LXsCATEGORY_SAMPLE, LXsP_SAMPLE_UNREAL);
		return LXe_OK;
	}

	/*
	* Attach to channel evaluations for per-sample values. This sets the indicies
	* for the channels in the evaluator. These are for the per-sample channels.
	*/
	LxResult UnrealMaterial::cmt_LinkSampleChannels(ILxUnknownID eval, ILxUnknownID item, int *idx)
	{
		// the index of any channel that is not driven will be set to LXiNODAL_NOT_DRIVEN
		nodalSvc.AddSampleChan(eval, item, idxs[0].chan, idx, LXfECHAN_READ);
		nodalSvc.AddSampleChan(eval, item, idxs[1].chan, idx, LXfECHAN_READ);

		nodalSvc.AddSampleChan(eval, item, idxs[2].chan, idx, LXfECHAN_READ);
		nodalSvc.AddSampleChan(eval, item, idxs[3].chan, idx, LXfECHAN_READ);
		nodalSvc.AddSampleChan(eval, item, idxs[4].chan, idx, LXfECHAN_READ);

		nodalSvc.AddSampleChan(eval, item, idxs[5].chan, idx, LXfECHAN_READ);
		nodalSvc.AddSampleChan(eval, item, idxs[6].chan, idx, LXfECHAN_READ);
		nodalSvc.AddSampleChan(eval, item, idxs[7].chan, idx, LXfECHAN_READ);

		nodalSvc.AddSampleChan(eval, item, idxs[8].chan, idx, LXfECHAN_READ);
		nodalSvc.AddSampleChan(eval, item, idxs[9].chan, idx, LXfECHAN_READ);
		nodalSvc.AddSampleChan(eval, item, idxs[10].chan, idx, LXfECHAN_READ);

		nodalSvc.AddSampleChan(eval, item, idxs[11].chan, idx, LXfECHAN_READ);
		nodalSvc.AddSampleChan(eval, item, idxs[12].chan, idx, LXfECHAN_READ);

		nodalSvc.AddSampleChan(eval, item, idxs[13].chan, idx, LXfECHAN_READ);

		nodalSvc.AddSampleChan(eval, item, idxs[14].chan, idx, LXfECHAN_READ);
		nodalSvc.AddSampleChan(eval, item, idxs[15].chan, idx, LXfECHAN_READ);
		nodalSvc.AddSampleChan(eval, item, idxs[16].chan, idx, LXfECHAN_READ);

		return LXe_OK;
	}

	/*
	* Read channel values which may have changed.
	*/
	LxResult UnrealMaterial::cmt_ReadChannels(ILxUnknownID attr, void **ppvData)
	{
		CLxUser_Attributes at(attr);
		RendData* rd = new RendData;

		rd->shaderMode = at.Bool(idxs[0].layer);
		rd->bakingMode = at.Bool(idxs[1].layer);

		rd->baseColor[0] = at.Float(idxs[2].layer);
		rd->baseColor[1] = at.Float(idxs[3].layer);
		rd->baseColor[2] = at.Float(idxs[4].layer);

		rd->metallic = at.Float(idxs[5].layer);
		rd->specular = at.Float(idxs[6].layer);
		rd->roughness = at.Float(idxs[7].layer);

		rd->emissiveColor[0] = at.Float(idxs[8].layer);
		rd->emissiveColor[1] = at.Float(idxs[9].layer);
		rd->emissiveColor[2] = at.Float(idxs[10].layer);

		rd->uOpacity = at.Float(idxs[11].layer);
		rd->tessMultiplier = at.Float(idxs[12].layer);

		rd->channelDebug = at.Float(idxs[13].layer);

		rd->auxRGB[0] = at.Float(idxs[14].layer);
		rd->auxRGB[1] = at.Float(idxs[15].layer);
		rd->auxRGB[2] = at.Float(idxs[16].layer);

		ppvData[0] = rd;
		return LXe_OK;
	}

	/*
	* Shader needs shading values to be computed below it, so it can't be opaque.
	*/
	LxResult UnrealMaterial::cmt_SetOpaque( int *opaque)
	{
		*opaque = 0;
		return LXe_OK;
	}

	LxResult UnrealMaterial::cmt_CustomPacket( const char **packet)
	{
		packet[0] = LXsP_SAMPLE_UNREAL;
		return LXe_OK;
	}

	/*
	* Set custom material values at a spot
	*/
	void UnrealMaterial::cmt_MaterialEvaluate(ILxUnknownID etor, int *idx, ILxUnknownID vector, void *data)
	{
		#ifndef LERP
			#define LERP(x,y,a)	((x) + (a) * ((y) - (x)))
		#endif

		RendData* rd = (RendData*)data;
		LXpUnreal *sPacket = (LXpUnreal*)pkt_service.FastPacket(vector, pkt_offset);
		LXpSampleParms *sParms = (LXpSampleParms*)pkt_service.FastPacket(vector, prm_offset);
		LXpSampleDriver *sDriver = (LXpSampleDriver*)pkt_service.FastPacket(vector, drv_offset);
		LXpSampleMask *sMask = (LXpSampleMask*)pkt_service.FastPacket(vector, msk_offset);
		LXpDisplace *sDisp = (LXpDisplace*)pkt_service.FastPacket(vector, dis_offset);
		LXtVector zero = { 0, 0, 0 };
		LXtVector one = { 1, 1, 1 };

		// Material properties to always modify:
		sParms->flags |= LXfSURF_PHYSICAL;
		sParms->reflType = -1;
		sParms->flags |= LXfSURF_REFLBLUR;
		sParms->reflRays = 1024;
		sParms->lumiAmt = 1;

		if (rd->shaderMode)
		{
			// Custom property pre-processing.
			sPacket->baseColor[0] = LXxCLAMP(sPacket->baseColor[0], 0, 1);
			sPacket->baseColor[1] = LXxCLAMP(sPacket->baseColor[1], 0, 1);
			sPacket->baseColor[2] = LXxCLAMP(sPacket->baseColor[2], 0, 1);

			sPacket->metallic = LXxCLAMP(sPacket->metallic, 0, 1);
			sPacket->specular = LXxCLAMP(sPacket->specular, 0, 1);
			sPacket->roughness = LXxCLAMP(sPacket->roughness, 0, 1);

			sPacket->emissiveColor[0] = LXxCLAMP(sPacket->emissiveColor[0], 0, 1);
			sPacket->emissiveColor[1] = LXxCLAMP(sPacket->emissiveColor[1], 0, 1);
			sPacket->emissiveColor[2] = LXxCLAMP(sPacket->emissiveColor[2], 0, 1);

			sPacket->uOpacity = LXxCLAMP(sPacket->uOpacity, 0, 1);
			sPacket->tessMultiplier = LXxCLAMP(sPacket->tessMultiplier, 0, 1);

			sPacket->auxRGB[0] = LXxCLAMP(sPacket->auxRGB[0], 0, 1);
			sPacket->auxRGB[1] = LXxCLAMP(sPacket->auxRGB[1], 0, 1);
			sPacket->auxRGB[2] = LXxCLAMP(sPacket->auxRGB[2], 0, 1);

			float mo = LXxCLAMP(sPacket->specular * 2, 0, 1);			// Micro occlusion darkening.

			if (rd->bakingMode)
			{
				sParms->specFres = 0;
				sParms->reflFres = 0;

				//LXx_VSET3(sParms->diffCol, sPacket->baseColor[0] * mo, sPacket->baseColor[1] * mo, sPacket->baseColor[2] * mo);
				LXx_VSET3(sParms->diffCol, sPacket->baseColor[0], sPacket->baseColor[1], sPacket->baseColor[2]);
				LXx_VSET3(sParms->specCol, sPacket->auxRGB[0], sPacket->auxRGB[1], sPacket->auxRGB[2]);
				LXx_VSET3(sParms->reflCol, 1, 1, 1);
				LXx_VSET3(sParms->lumiCol, sPacket->emissiveColor[0], sPacket->emissiveColor[1], sPacket->emissiveColor[2]);
				LXx_VSET3(sParms->subsCol, sPacket->metallic, sPacket->roughness, sPacket->specular);
				LXx_VSET3(sParms->tranCol, sPacket->uOpacity, sPacket->tessMultiplier, 0);

				sParms->diffAmt = 1;
				sParms->specAmt = 1;
				sParms->rough = sPacket->roughness;
				sParms->reflAmt = sPacket->metallic;
				sParms->tranAmt = sPacket->uOpacity;
				sDriver->a = sPacket->specular;
				sDriver->b = sPacket->tessMultiplier;
			}
			else
			{
				sParms->specFres = 1;
				sParms->reflFres = 1;

				sPacket->metallic = pow(sPacket->metallic, 3);

				LXtVector rc1 = { sPacket->baseColor[0], sPacket->baseColor[1], sPacket->baseColor[2] };
				LXx_VSET3(sParms->diffCol, rc1[0] * mo, rc1[1] * mo, rc1[2] * mo);			// Diffuse color.
				LXtVector rc2 = { 1 - rc1[0], 1 - rc1[1], 1 - rc1[2] };
				LXtVector rc3;
				LXx_VLERP(rc3, one, rc2, 0.5);								// Diffuse color independent, always white specular.
				LXtVector rc31;
				LXx_VLERP(rc31, rc3, rc1, sPacket->metallic);				// Blend from white to colored specular.
				LXx_VSET3(sParms->specCol, rc31[0], rc31[1], rc31[2]);		// Specular color.
				LXtVector rc4;
				LXx_VLERP(rc4, rc3, rc1, sPacket->metallic);				// Blending from white to colored reflection.
				LXx_VSET3(sParms->reflCol, rc4[0], rc4[1], rc4[2]);			// Reflection color.
				LXx_VSET3(sParms->lumiCol, sPacket->emissiveColor[0], sPacket->emissiveColor[1], sPacket->emissiveColor[2]);	// Emissive color.

				sParms->diffAmt = LERP(1.0, 0.5, sPacket->metallic);		// Diffuse amount.
				float sa1 = 1 - pow(sPacket->roughness, 0.5);
				float sa2 = LERP(0.1, 1, sa1);
				float sa11 = 1 - pow(sPacket->roughness, 0.7);
				float sa12 = LERP(0.05, 0.1, sa11) * (sPacket->specular * 2);
				sParms->specAmt = LERP(sa12, sa2, sPacket->metallic);					// Specular amount.

				float sr1 = LXxCLAMP(sPacket->roughness * 4, 0, 1);
				float sr2 = LERP(0.08, 0.008, sr1);
				float sr3 = pow(sPacket->roughness, sr2);
				sParms->rough = sr3;													// Roughness.
				float sr4 = LERP(sr3, pow(sr3, 0.5), sPacket->metallic);
				sParms->specExpU = LERP(10000, 1, sr4);									// Specular exponent U.
				sParms->specExpV = LERP(10000, 1, sr4);									// Specular exponent V.

				float ra1 = LERP(1, 0.5, sPacket->roughness);
				float ra2 = LERP(0, 1, pow(sPacket->specular, 2.248) * 0.19);			// Should be 0.04 at 0.5.
				sParms->reflAmt = LERP(ra2, ra1, sPacket->metallic);					// Reflection amount.
				sParms->dissAmt = 1 - sPacket->uOpacity;								// Dissolve.
				sDriver->a = sPacket->specular;											// Driver A.
				sDriver->b = sPacket->tessMultiplier;									// Driver B.

				// DEBUG
				//LXx_VSET3(sParms->diffCol, sParms->reflAmt, sParms->reflAmt, sParms->reflAmt);
			}
		}
		else // Material mode
		{
			sPacket->baseColor[0] = nodalSvc.GetFloat(etor, idx, idxs[2].chan, rd->baseColor[0]);
			sPacket->baseColor[1] = nodalSvc.GetFloat(etor, idx, idxs[3].chan, rd->baseColor[1]);
			sPacket->baseColor[2] = nodalSvc.GetFloat(etor, idx, idxs[4].chan, rd->baseColor[2]);

			sPacket->metallic = nodalSvc.GetFloat(etor, idx, idxs[5].chan, rd->metallic);
			sPacket->specular = nodalSvc.GetFloat(etor, idx, idxs[6].chan, rd->specular);
			sPacket->roughness = nodalSvc.GetFloat(etor, idx, idxs[7].chan, rd->roughness);

			sPacket->emissiveColor[0] = nodalSvc.GetFloat(etor, idx, idxs[8].chan, rd->emissiveColor[0]);
			sPacket->emissiveColor[1] = nodalSvc.GetFloat(etor, idx, idxs[9].chan, rd->emissiveColor[1]);
			sPacket->emissiveColor[2] = nodalSvc.GetFloat(etor, idx, idxs[10].chan, rd->emissiveColor[2]);

			sPacket->uOpacity = nodalSvc.GetFloat(etor, idx, idxs[11].chan, rd->uOpacity);
			sPacket->tessMultiplier = nodalSvc.GetFloat(etor, idx, idxs[12].chan, rd->tessMultiplier);
			// idxs[12] is Channel debug.
			sPacket->auxRGB[0] = nodalSvc.GetFloat(etor, idx, idxs[14].chan, rd->auxRGB[0]);
			sPacket->auxRGB[1] = nodalSvc.GetFloat(etor, idx, idxs[15].chan, rd->auxRGB[1]);
			sPacket->auxRGB[2] = nodalSvc.GetFloat(etor, idx, idxs[16].chan, rd->auxRGB[2]);
		}
	}

	/*
	* Evaluate the color at a spot.
	*/
	void UnrealMaterial::cmt_ShaderEvaluate(ILxUnknownID vector, ILxUnknownID rayObj, LXpShadeComponents *sCmp, LXpShadeOutput *sOut, void *data)
	{
		LXpUnreal *sPacket = (LXpUnreal*)pkt_service.FastPacket(vector, pkt_offset);
		LXpDisplace *sDisp = (LXpDisplace*)pkt_service.FastPacket(vector, dis_offset);
		RendData *rd = (RendData *)data;
		float newDiff = 0;
		float terminatorTightness = 2;

		channelDebugMode = LXxCLAMP(rd->channelDebug, 0, DEBUG_END - 1);

		switch (channelDebugMode)
		{
		case DEBUG_BASECOLOR:
			for (int i = 0; i < 3; i++) { sOut->color[i] = sPacket->baseColor[i]; }
			break;
		case DEBUG_METALLIC:
			for (int i = 0; i < 3; i++) { sOut->color[i] = sPacket->metallic; }
			break;
		case DEBUG_SPECULAR:
			for (int i = 0; i < 3; i++) { sOut->color[i] = sPacket->specular; }
			break;
		case DEBUG_ROUGHNESS:
			for (int i = 0; i < 3; i++) { sOut->color[i] = sPacket->roughness; }
			break;
		case DEBUG_EMISSIVECOLOR:
			for (int i = 0; i < 3; i++) { sOut->color[i] = sPacket->emissiveColor[i]; }
			break;
		case DEBUG_OPACITY:
			for (int i = 0; i < 3; i++) { sOut->color[i] = sPacket->uOpacity; }
			break;
		case DEBUG_TESSMULTIPLIER:
			for (int i = 0; i < 3; i++) { sOut->color[i] = sPacket->tessMultiplier; }
			break;
		case DEBUG_AUXRGB:
			for (int i = 0; i < 3; i++) { sOut->color[i] = sPacket->auxRGB[i]; }
			break;
		default:
			for (int i = 0; i < 3; i++)
			{
				newDiff = LERP(terminatorTightness * sCmp->diff[i], sCmp->diff[i], LXxCLAMP(sCmp->diff[i], 0, 1));
				sOut->color[i] = newDiff + sCmp->spec[i] + sCmp->refl[i] + sCmp->tran[i] + sCmp->subs[i] + sCmp->lumi[i];
			}
			break;
		}
	}

	/*
	* Utility to get the type code for this item type, as needed.
	*/
	LXtItemType UnrealMaterial::MyType()
	{
		if (my_type != LXiTYPE_NONE)
			return my_type;

		CLxUser_SceneService	 svc;

		my_type = svc.ItemType(SRVs_UNREAL_MATR_ITEMTYPE);
		return my_type;
	}

	LxResult UnrealMaterial::cui_UIHints(const char *channelName, ILxUnknownID hints)
	{
		if (strcmp(channelName, UNREAL_CH_SHADERMODE) != 0.0 &&
			strcmp(channelName, UNREAL_CH_BAKINGMODE) != 0.0 &&
			strcmp(channelName, UNREAL_CH_BASECOLOR) != 0.0 &&
			strcmp(channelName, UNREAL_CH_EMISSIVECOLOR) != 0.0 &&
			strcmp(channelName, UNREAL_CH_CHANNELDEBUG) != 0.0 &&
			strcmp(channelName, UNREAL_CH_AUXRGB) != 0.0)
		{
			CLxUser_UIHints		 ui(hints);
			//ui.ChannelFlags(LXfUIHINTCHAN_SUGGESTED);
			ui.MinFloat(0.0);
			ui.MaxFloat(1.0);
			return LXe_OK;
		}

		return LXe_NOTIMPL;
	}
	LxResult UnrealMaterial::cui_Enabled(const char *channelName, ILxUnknownID msg, ILxUnknownID item, ILxUnknownID read)
	{
		CLxUser_Item		 src(item);
		CLxUser_ChannelRead	 chan(read);

		// If the channel is not "ShaderMode" and "ShaderMode" is not 0 (so it's true)...
		if (!strcmp(channelName, UNREAL_CH_SHADERMODE) == 0.0 && chan.IValue(src, UNREAL_CH_SHADERMODE) != 0.0)
		{
			// If channel is "BakingMode"...
			if (strcmp(channelName, UNREAL_CH_BAKINGMODE) == 0.0 ||
				strcmp(channelName, UNREAL_CH_CHANNELDEBUG) == 0.0) return LXe_OK;
			else return LXe_CMD_DISABLED;
		}
			
		// If channel is "BakingMode"...
		if (strcmp(channelName, UNREAL_CH_BAKINGMODE) == 0.0 ||
			strcmp(channelName, UNREAL_CH_CHANNELDEBUG) == 0.0) return LXe_CMD_DISABLED;
		else return LXe_OK;
	}

	/* --------------------------------- */

	/*
	* Packet Effects definition:
	* Here we define the texture effects on the Unreal packet
	*/

	class UnrealPFX : public CLxImpl_PacketEffect
	{
	public:
		UnrealPFX() {}

		static LXtTagInfoDesc	descInfo[];

		LxResult		pfx_Packet(const char **packet) LXx_OVERRIDE;
		unsigned int		pfx_Count(void) LXx_OVERRIDE;
		LxResult		pfx_ByIndex(int idx, const char **name, const char **typeName, int	*type) LXx_OVERRIDE;
		LxResult		pfx_Get(int idx, void *packet, float *val, void *item) LXx_OVERRIDE;
		LxResult		pfx_Set(int idx, void *packet, const float *val, void *item) LXx_OVERRIDE;
	};

#define SRVs_UNREAL_PFX		"UnrealShader"
#define SRVs_BASECOLOR_TFX		"unrealBaseColor"
#define SRVs_METALLIC_TFX		"unrealMetallic"
#define SRVs_SPECULAR_TFX		"unrealSpecular"
#define SRVs_ROUGHNESS_TFX		"unrealRoughness"
#define SRVs_EMISSIVECOLOR_TFX	"unrealEmissiveColor"
#define SRVs_OPACITY_TFX		"unrealOpacity"
#define SRVs_TESSMULTIPLIER_TFX	"unrealTessMultiplier"
#define SRVs_AUXRGB_TFX			"unrealAuxRGB"

	LXtTagInfoDesc UnrealPFX::descInfo[] =
	{
			{ LXsSRV_USERNAME, "Unreal Packet FX" },
			{ LXsSRV_LOGSUBSYSTEM, "texture-effect" },
			{ LXsTFX_CATEGORY, LXsSHADE_SURFACE },
			{ 0 }
	};

	LxResult UnrealPFX::pfx_Packet(const char	**packet)
	{
		packet[0] = SRVs_UNREAL_VPACKET;
		return LXe_OK;
	}

	unsigned int UnrealPFX::pfx_Count(void)
	{
		return	8;
	}

	LxResult UnrealPFX::pfx_ByIndex(int	id, const char **name, const char **typeName, int *type)
	{
		switch (id)
		{
			case 0:
				name[0] = SRVs_BASECOLOR_TFX;
				type[0] = LXi_TFX_COLOR | LXf_TFX_READ | LXf_TFX_WRITE;
				typeName[0] = LXsTYPE_COLOR1;
				break;
			case 1:
				name[0] = SRVs_METALLIC_TFX;
				type[0] = LXi_TFX_SCALAR | LXf_TFX_READ | LXf_TFX_WRITE;
				typeName[0] = LXsTYPE_FLOAT;
				break;
			case 2:
				name[0] = SRVs_SPECULAR_TFX;
				type[0] = LXi_TFX_SCALAR | LXf_TFX_READ | LXf_TFX_WRITE;
				typeName[0] = LXsTYPE_FLOAT;
				break;
			case 3:
				name[0] = SRVs_ROUGHNESS_TFX;
				type[0] = LXi_TFX_SCALAR | LXf_TFX_READ | LXf_TFX_WRITE;
				typeName[0] = LXsTYPE_FLOAT;
				break;
			case 4:
				name[0] = SRVs_EMISSIVECOLOR_TFX;
				type[0] = LXi_TFX_COLOR | LXf_TFX_READ | LXf_TFX_WRITE;
				typeName[0] = LXsTYPE_COLOR1;
				break;
			case 5:
				name[0] = SRVs_OPACITY_TFX;
				type[0] = LXi_TFX_SCALAR | LXf_TFX_READ | LXf_TFX_WRITE;
				typeName[0] = LXsTYPE_FLOAT;
				break;
			case 6:
				name[0] = SRVs_TESSMULTIPLIER_TFX;
				type[0] = LXi_TFX_SCALAR | LXf_TFX_READ | LXf_TFX_WRITE;
				typeName[0] = LXsTYPE_FLOAT;
				break;
			case 7:
				name[0] = SRVs_AUXRGB_TFX;
				type[0] = LXi_TFX_COLOR | LXf_TFX_READ | LXf_TFX_WRITE;
				typeName[0] = LXsTYPE_COLOR1;
				break;
		}

		return	LXe_OK;
	}

	LxResult UnrealPFX::pfx_Get(int  id, void *packet, float *val, void *item)
	{
		LXpUnreal	*csp = (LXpUnreal *)packet;

		switch (id)
		{
			case 0:
				val[0] = csp->baseColor[0];
				val[1] = csp->baseColor[1];
				val[2] = csp->baseColor[2];
				break;
			case 1:
				val[0] = csp->metallic;
				break;
			case 2:
				val[0] = csp->specular;
				break;
			case 3:
				val[0] = csp->roughness;
				break;
			case 4:
				val[0] = csp->emissiveColor[0];
				val[1] = csp->emissiveColor[1];
				val[2] = csp->emissiveColor[2];
				break;
			case 5:
				val[0] = csp->uOpacity;
				break;
			case 6:
				val[0] = csp->tessMultiplier;
				break;
			case 7:
				val[0] = csp->auxRGB[0];
				val[1] = csp->auxRGB[1];
				val[2] = csp->auxRGB[2];
				break;
		}

		return LXe_OK;
	}

	LxResult UnrealPFX::pfx_Set(int  id, void *packet, const float *val, void *item)
	{
		LXpUnreal	*csp = (LXpUnreal *)packet;

		switch (id)
		{
			case 0:
				csp->baseColor[0] = val[0];
				csp->baseColor[1] = val[1];
				csp->baseColor[2] = val[2];
				break;
			case 1:
				csp->metallic = val[0];
				break;
			case 2:
				csp->specular = val[0];
				break;
			case 3:
				csp->roughness = val[0];
				break;
			case 4:
				csp->emissiveColor[0] = val[0];
				csp->emissiveColor[1] = val[1];
				csp->emissiveColor[2] = val[2];
				break;
			case 5:
				csp->uOpacity = val[0];
				break;
			case 6:
				csp->tessMultiplier = val[0];
				break;
			case 7:
				csp->auxRGB[0] = val[0];
				csp->auxRGB[1] = val[1];
				csp->auxRGB[2] = val[2];
				break;
		}

		return LXe_OK;
	}

	void initialize()
	{
		CLxGenericPolymorph*    srv1 = new CLxPolymorph<UnrealMaterial>;
		CLxGenericPolymorph*    srv2 = new CLxPolymorph<UnrealPacket>;
		CLxGenericPolymorph*    srv3 = new CLxPolymorph<UnrealPFX>;

		srv1->AddInterface(new CLxIfc_CustomMaterial<UnrealMaterial>);
		srv1->AddInterface(new CLxIfc_ChannelUI   <UnrealMaterial>);
		srv1->AddInterface(new CLxIfc_StaticDesc<UnrealMaterial>);
		lx::AddServer(SRVs_UNREAL_MATR, srv1);

		srv2->AddInterface(new CLxIfc_VectorPacket<UnrealPacket>);
		srv2->AddInterface(new CLxIfc_StaticDesc<UnrealPacket>);
		lx::AddServer(SRVs_UNREAL_VPACKET, srv2);

		srv3->AddInterface(new CLxIfc_PacketEffect<UnrealPFX>);
		srv3->AddInterface(new CLxIfc_StaticDesc<UnrealPFX>);
		lx::AddServer(SRVs_UNREAL_PFX, srv3);

	}

};	// END namespace

