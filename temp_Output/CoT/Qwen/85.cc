#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/config-store-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/buildings-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-enb-net-device.h"
#include "ns3/mmwave-ue-net-device.h"
#include "ns3/three-gpp-channel-model.h"
#include "ns3/three-gpp-spectrum-propagation-loss.h"
#include "ns3/spectrum-module.h"
#include "ns3/antenna-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularChannelSimulation");

class SimulationHelper : public Object
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("SimulationHelper")
      .SetParent<Object>()
      ;
    return tid;
  }

  void LogPathlossAndSNR(Ptr<const SpectrumValue> rxPsd, Ptr<SpectrumPhy> txPhy, Ptr<SpectrumPhy> rxPhy, Time duration, std::ofstream &outFile)
  {
    double pathlossDb = 0.0;
    double snrLinearSum = 0.0;
    uint32_t count = 0;

    for (Values::const_iterator vit = rxPsd->ConstValuesBegin(); vit != rxPsd->ConstValuesEnd(); ++vit)
    {
      double rxPowerW = (*vit);
      double noiseFigure = DynamicCast<MmWaveUeNetDevice>(rxPhy->GetDevice())->GetNoiseFigure();
      double kT = 1.38064852e-23 * 290; // Joules
      double noisePowerW = kT * rxPhy->GetSpectrumModel()->GetResolution().GetDouble() * pow(10, noiseFigure / 10.0);
      double snr = rxPowerW / noisePowerW;
      snrLinearSum += snr;
      count++;
    }

    double avgSNRdB = 10 * log10(snrLinearSum / count);
    outFile << Simulator::Now().GetSeconds() << " " << pathlossDb << " " << avgSNRdB << std::endl;
    Simulator::Schedule(duration, &SimulationHelper::LogPathlossAndSNR, this, rxPsd, txPhy, rxPhy, duration, outFile);
  }
};

int main(int argc, char *argv[])
{
  double simTime = 10.0;
  double loggingInterval = 0.1;
  std::string scenario = "UMa";
  std::string outputFileName = "vehicular_simulation_output.txt";

  CommandLine cmd(__FILE__);
  cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
  cmd.AddValue("scenario", "3GPP scenario: UMa, RMa, or Hilly", scenario);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::ThreeGppChannelModel::Scenario", StringValue(scenario));
  Config::SetDefault("ns3::ThreeGppChannelModel::Frequency", DoubleValue(5.9e9)); // 5.9 GHz
  Config::SetDefault("ns3::ThreeGppSpectrumPropagationLossModel::UpdatePeriod", TimeValue(Seconds(0.1)));
  Config::SetDefault("ns3::MmWaveHelper::UseBuildings", BooleanValue(false));

  NodeContainer enbNodes;
  NodeContainer ueNodes;
  enbNodes.Create(1);
  ueNodes.Create(2);

  MobilityHelper mobilityEnb;
  mobilityEnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue(0.0),
                                   "MinY", DoubleValue(0.0),
                                   "DeltaX", DoubleValue(5.0),
                                   "DeltaY", DoubleValue(5.0),
                                   "GridWidth", UintegerValue(3),
                                   "LayoutType", StringValue("RowFirst"));
  mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityEnb.Install(enbNodes);

  MobilityHelper mobilityUe;
  mobilityUe.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("100.0"),
                                  "Y", StringValue("0.0"),
                                  "Radius", StringValue("100.0"));
  mobilityUe.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobilityUe.Install(ueNodes);

  for (NodeContainer::Iterator it = ueNodes.Begin(); it != ueNodes.End(); ++it)
  {
    Ptr<ConstantVelocityMobilityModel> velModel = (*it)->GetObject<ConstantVelocityMobilityModel>();
    velModel->SetVelocity(Vector(20, 0, 0)); // 20 m/s in x-direction
  }

  MmWaveHelper mmWaveHelper;
  mmWaveHelper.Initialize();

  NetDeviceContainer enbDevs = mmWaveHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs = mmWaveHelper.InstallUeDevice(ueNodes);

  InternetStackHelper internet;
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = ipv4.Assign(ueDevs);

  mmWaveHelper.AttachToClosestEnb(ueDevs, enbDevs);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults();

  for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
  {
    Ptr<MmWaveUeNetDevice> ueDev = DynamicCast<MmWaveUeNetDevice>(ueDevs.Get(i));
    Ptr<UniformPlanarArray> antennaArray = CreateObject<UniformPlanarArray>();
    antennaArray->SetNumElements(16, 1); // 16 elements linear array
    antennaArray->SetAntennaElement(CreateObject<IsotropicAntennaModel>());
    ueDev->SetAntennaArray(antennaArray);
    ueDev->SetBeamformingAlgorithmType("ns3::MmWaveDftBeamforming");
  }

  for (uint32_t i = 0; i < enbDevs.GetN(); ++i)
  {
    Ptr<MmWaveEnbNetDevice> enbDev = DynamicCast<MmWaveEnbNetDevice>(enbDevs.Get(i));
    Ptr<UniformPlanarArray> antennaArray = CreateObject<UniformPlanarArray>();
    antennaArray->SetNumElements(64, 1);
    antennaArray->SetAntennaElement(CreateObject<IsotropicAntennaModel>());
    enbDev->SetAntennaArray(antennaArray);
    enbDev->SetBeamformingAlgorithmType("ns3::MmWaveDftBeamforming");
  }

  std::ofstream outFile(outputFileName.c_str());
  outFile << "# Time(s) AvgPathloss(dB) AvgSNR(dB)" << std::endl;

  SimulationHelper helper;
  for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
  {
    Ptr<MmWaveUeNetDevice> ueDev = DynamicCast<MmWaveUeNetDevice>(ueDevs.Get(i));
    Ptr<SpectrumPhy> uePhy = ueDev->GetPhy();
    Simulator::Schedule(Seconds(0.1), &SimulationHelper::LogPathlossAndSNR, &helper,
                        uePhy->GetRxPowerSpectralDensity(), nullptr, uePhy, Seconds(loggingInterval), outFile);
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();

  outFile.close();

  return 0;
}