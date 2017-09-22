// Metallic sound generator
// (c) 2017 Arvīds Kokins
// - size of output data specified by NUM_WAVE_SAMPLES

#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include <assert.h>

#if 1
#  define REAL double
#else
#  define REAL float
#endif
#  define RC(x) ((REAL)(x))

#define SAMPLES_X 32 // EDITABLE: seems to slightly affect the "shape" of the sound
#define SAMPLES_Y 64 // ^
#define NORM_SAMPLES (RC(1)/(SAMPLES_X*SAMPLES_Y))

#define IMPULSE RC(5000.0)
#define RIGIDITY RC(2000.0) // this should be less than half of inverse timestep for the simulation to be stable
#define CENTER_RIGIDITY RC(5000.0)
#define DAMPING RC(0.999)
#define TIMESTEP (RC(1)/RC(11025)) // EDITABLE: try changing this for frequency modification

#define NUM_SUBITERATIONS 4
#define NORM_SUBITERATIONS (RC(1)/NUM_SUBITERATIONS)

REAL samples_p[ SAMPLES_X * SAMPLES_Y ];
REAL samples_v[ SAMPLES_X * SAMPLES_Y ];
REAL rigidities[ SAMPLES_X * SAMPLES_Y ];
REAL dampings[ SAMPLES_X * SAMPLES_Y ];

static void init()
{
	for( int y = 0; y < SAMPLES_Y; ++y )
	{
		REAL fy = RC(y) / RC(SAMPLES_Y - 1) * RC(2) - RC(1);
		int yoff = y * SAMPLES_X;
		for( int x = 0; x < SAMPLES_X; ++x )
		{
			REAL fx = RC(x) / RC(SAMPLES_X - 1) * RC(2) - RC(1);
			REAL dist = sqrtf( fx * fx + fy * fy );
			rigidities[ yoff + x ] = RIGIDITY / ( 1 + dist );
			dampings[ yoff + x ] = DAMPING - dist * RC(0.005); // EDITABLE: this controls rolloff speed 
		}
	}
}

static void solve()
{
	REAL dt = TIMESTEP;
	
	// individual particles
	for( int i = 0; i < SAMPLES_X * SAMPLES_Y; ++i )
	{
		// position simulation
		samples_p[ i ] += samples_v[ i ] * dt;
		
		// 0-spring [DISABLED]
	//	REAL force = -samples_p[ i ] * RIGIDITY - samples_v[ i ] * DAMPING;
	//	samples_v[ i ] += force * dt;
	}
	
	// what has been tried and didn't work so well:
	// - position += noise;
	// - velocity += noise;
	// ^ in both cases, the noise was too obvious, could be more beneficial to add it during post-processing
	
	// center hold
	{
		int x = SAMPLES_X / 2;
		int y = SAMPLES_Y / 2;
		int i = x + y * SAMPLES_X;
		REAL force = -samples_p[ i ] * CENTER_RIGIDITY - samples_v[ i ] * dampings[ i ];
		samples_v[ i ] += force * dt;
	}
	
	// joints [X/horizontal]
	for( int y = 0; y < SAMPLES_Y; ++y )
	{
		int yoff = y * SAMPLES_X;
		for( int x = 0; x < SAMPLES_X - 1; ++x )
		{
			REAL diff_p = samples_p[ yoff + x + 1 ] - samples_p[ yoff + x ];
			REAL diff_v = samples_v[ yoff + x + 1 ] - samples_v[ yoff + x ];
			REAL force = -diff_p * rigidities[ yoff + x ] - diff_v * dampings[ yoff + x ] * RC(0.5) * dt; // 0.5 because applying to both samples
			samples_v[ yoff + x ] -= force;
			samples_v[ yoff + x + 1 ] += force;
		}
	}
	
	// joints [Y/vertical]
	for( int y = 0; y < SAMPLES_Y - 1; ++y )
	{
		int yoff = y * SAMPLES_X;
		for( int x = 0; x < SAMPLES_X; ++x )
		{
			REAL diff_p = samples_p[ yoff + x + SAMPLES_X ] - samples_p[ yoff + x ];
			REAL diff_v = samples_v[ yoff + x + SAMPLES_X ] - samples_v[ yoff + x ];
			REAL force = -diff_p * rigidities[ yoff + x ] - diff_v * dampings[ yoff + x ] * RC(0.5) * dt; // 0.5 because applying to both samples
			samples_v[ yoff + x ] -= force;
			samples_v[ yoff + x + SAMPLES_X ] += force;
		}
	}
}

static REAL getsample()
{
#if NOT_THIS
	// this seems to produce a low-frequency sine wave
	REAL v = 0;
	for( int i = 0; i < SAMPLES_X * SAMPLES_Y; ++i )
		v += samples_p[ i ];
	return v * NORM_SAMPLES;
#elif 1 // ------------ USING THIS ONE
	// subtract low-frequency component from impulse-struck sample
	REAL v = 0;
	for( int i = 0; i < SAMPLES_X * SAMPLES_Y; ++i )
		v += samples_p[ i ];
	return samples_p[ 16 ] - v * NORM_SAMPLES;
#else
	// the impulse-struck sample, contains too much low-frequency data
	return samples_p[ 16 ];
#endif
}



#define WAVE_SAMPLE_RATE 44100
#define NUM_WAVE_SAMPLES (WAVE_SAMPLE_RATE * 2) // 2 seconds

int16_t output_wave[ NUM_WAVE_SAMPLES ];

static void write_u32( FILE* fp, uint32_t v )
{
	fwrite( &v, 1, 4, fp );
}
static void write_u16( FILE* fp, uint16_t v )
{
	fwrite( &v, 1, 2, fp );
}

static void write_to_wav( const char* filename )
{
	FILE* fp = fopen( filename, "wb" );
	assert( fp );
	
	fwrite( "RIFF", 1, 4, fp );
	write_u32( fp, 36 + sizeof( output_wave ) ); // chunk size
	fwrite( "WAVE", 1, 4, fp );
	
	fwrite( "fmt ", 1, 4, fp );
	write_u32( fp, 16 ); // chunk size
	write_u16( fp, 1 ); // PCM format
	write_u16( fp, 1 ); // # channels
	write_u32( fp, WAVE_SAMPLE_RATE );
	write_u32( fp, WAVE_SAMPLE_RATE * 2 ); // byte rate
	write_u16( fp, 2 ); // # channels * bytes per sample / block align
	write_u16( fp, 16 ); // bits per sample
	
	fwrite( "data", 1, 4, fp );
	write_u32( fp, sizeof( output_wave ) ); // chunk size
	fwrite( output_wave, 1, sizeof( output_wave ), fp );
	
	fclose( fp );
}



int main()
{
	init();
	
	// impulse (simulates collision that invokes further vibration)
	samples_v[ 16 ] = IMPULSE;
	
	puts( "simulating..." );
	for( int i = 0; i < NUM_WAVE_SAMPLES; ++i )
	{
		if( i % 1000 == 999 )
		{
			printf( "%.1f%%\n", 100.f * i / (REAL) NUM_WAVE_SAMPLES );
		//	printf( "sample = %f; velocity = %f\n", samples_p[ 16 ], samples_v[ 16 ] );
		}
		
		REAL v = 0;
		
		for( int j = 0; j < NUM_SUBITERATIONS; ++j )
		{
			solve();
			v += getsample();
		}
		
		v *= NORM_SUBITERATIONS;
		
		if( v < -1 )
			v = -1;
		else if( v > 1 )
			v = 1;
		output_wave[ i ] = v * 32767;
	}
	
	write_to_wav( "metallic.wav" );
	
	return 0;
}
