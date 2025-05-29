#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/udp-client-server-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  uint32_t numDevices = 5;
  double simulationTime = 10;
  uint16_t port = 9;

  CommandLine cmd;
  cmd.AddValue("numDevices", "Number of IoT devices", numDevices);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("port", "UDP port number", port);
  cmd.Parse(argc, argv);

  NodeContainer devices, server;
  devices.Create(numDevices);
  server.Create(1);

  LrWpanHelper lrWpanHelper;
  NetDeviceContainer deviceNetDevices = lrWpanHelper.Install(devices);
  NetDeviceContainer serverNetDevices = lrWpanHelper.Install(server);

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDeviceNetDevices = sixLowPanHelper.Install(deviceNetDevices);
  NetDeviceContainer sixLowPanServerNetDevices = sixLowPanHelper.Install(serverNetDevices);

  InternetStackHelper internet;
  internet.Install(devices);
  internet.Install(server);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer deviceInterfaces = ipv6.Assign(sixLowPanDeviceNetDevices);
  Ipv6InterfaceContainer serverInterfaces = ipv6.Assign(sixLowPanServerNetDevices);

  serverInterfaces.SetForwarding(0, true);
  for (uint32_t i = 0; i < numDevices; ++i) {
    deviceInterfaces.SetForwarding(i, true);
  }

  UdpClientHelper client(serverInterfaces.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(1)));
  client.SetAttribute("PacketSize", UintegerValue(100));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numDevices; ++i) {
    clientApps.Add(client.Install(devices.Get(i)));
  }
  clientApps.Start(Seconds(2));
  clientApps.Stop(Seconds(simulationTime - 1));

  UdpServerHelper serverHelper(port);
  ApplicationContainer serverApps = serverHelper.Install(server.Get(0));
  serverApps.Start(Seconds(1));
  serverApps.Stop(Seconds(simulationTime));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}