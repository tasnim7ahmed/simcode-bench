#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpExample");

int main(int argc, char *argv[]) {
  // Enable logging for UdpClient and UdpServer
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Set the simulation start and stop times
  Time::SetResolution(Time::NS);
  Time simulationTime = Seconds(10.0);

  // Create two nodes: eNB and UE
  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Create LTE helper
  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType("ns3::CosineAntennaModel");
  lteHelper.SetSchedulerType (LteHelper::RR_SCHEDULER);

  // Create EPC helper
  Ptr<LteUeMeasurementsHandoverAlgorithm> hoAlgo =
      CreateObject<LteUeMeasurementsHandoverAlgorithm>();
  lteHelper.SetHandoverAlgorithm(hoAlgo);
  EpcHelper epcHelper;
  lteHelper.SetEpcHelper(epcHelper);

  // Install LTE devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  // Install the IP stack on the nodes
  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  // Assign IP addresses to the UE and eNB
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4h.Assign(enbLteDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4h.Assign(ueLteDevs);

  // Attach the UE to the eNB
  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  // Activate a data radio bearer between UE and eNB
  enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
  EpsBearer bearer(q);
  lteHelper.ActivateDataRadioBearer(ueNodes.Get(0), bearer, enbLteDevs.Get(0));

  // Set mobility model
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);
  mobility.Install(ueNodes);

  // Create UDP application: server
  uint16_t port = 12345;  // well-known port for server
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(simulationTime);

  // Create UDP application: client
  UdpClientHelper client(ueIpIface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(simulationTime);

  // Enable tracing
  // Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>("lte-udp-example.pcap", std::ios::out);
  // lteHelper.EnablePcapAll( "lte-udp-example", stream );

  // Run the simulation
  Simulator::Stop(simulationTime);
  Simulator::Run();

  // Cleanup
  Simulator::Destroy();

  return 0;
}