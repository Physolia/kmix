#ifndef MIXER_ALSA_H
#define MIXER_ALSA_H

// QT includes
#include <qvaluelist.h>

// Forward QT includes
class QString;

class Mixer_ALSA : public Mixer
{
	public:
		Mixer_ALSA( int device = -1, int card = -1 );
		~Mixer_ALSA();
		
		virtual int  readVolumeFromHW( int devnum, Volume &vol );
		virtual int  writeVolumeToHW( int devnum, Volume &vol );
		virtual bool setRecsrcHW( int devnum, bool on);
		virtual bool isRecsrcHW( int devnum );
	        virtual void setEnumIdHW(int mixerIdx, unsigned int);
      		virtual unsigned int enumIdHW(int mixerIdx);
		virtual bool prepareUpdate();

		
	protected:
		virtual int	openMixer();
		virtual int releaseMixer();
		
	private:
		int identify( snd_mixer_selem_id_t *sid );
		snd_mixer_elem_t* getMixerElem(int devnum);

		virtual QString errorText(int mixer_error);
		//typedef QValueList<snd_mixer_selem_id_t *>AlsaMixerSidList;
		//AlsaMixerSidList mixer_sid_list;
		typedef QValueList<snd_mixer_elem_t *> AlsaMixerElemList;
		AlsaMixerElemList mixer_elem_list;

                bool _initialUpdate;
		snd_mixer_t *_handle;
		QString devName;
};

#endif
