import HeroSection from '@/components/home/HeroSection';
import StatsBar from '@/components/home/StatsBar';
import HowItWorks from '@/components/home/HowItWorks';
import FeatureGrid from '@/components/home/FeatureGrid';
import HardwareSection from '@/components/home/HardwareSection';
import RoadmapSection from '@/components/home/RoadmapSection';
import CtaSection from '@/components/home/CtaSection';

export default function HomePage() {
  return (
    <>
      <HeroSection />
      <StatsBar />
      <HowItWorks />
      <FeatureGrid />
      <HardwareSection />
      <RoadmapSection />
      <CtaSection />
    </>
  );
}
