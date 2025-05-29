#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lorawan-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/command-line.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LoraWanSensorExample");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("LoraWanSensorExample", LOG_LEVEL_INFO);

    // Set up command line arguments
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Configure simulation parameters
    double simulationTime = 10; // seconds
    double appPeriodSeconds = 1;  // seconds

    // Create the channel
    LoraChannelHelper channelHelper;
    Ptr<LoraChannel> channel = channelHelper.Create();

    // Create the LoRaWAN device helpers
    LoraWanPhyHelper phyHelper = LoraWanPhyHelper::Default();
    phyHelper.SetChannel(channel);

    LoraWanMacHelper macHelper = LoraWanMacHelper::Default();
    LoraWanHelper lorawanHelper = LoraWanHelper::Default();

    // Create the LoRaWAN network
    NodeContainer endDevices;
    endDevices.Create(1);

    NodeContainer gateways;
    gateways.Create(1);

    // Configure the LoRaWAN end device
    lorawanHelper.ConfigureEndDevices(endDevices);

    // Configure the LoRaWAN gateway
    lorawanHelper.ConfigureGateways(gateways);

    // Install the LoRaWAN devices
    NetDeviceContainer endDeviceDevices = lorawanHelper.InstallEndDevices(endDevices, phyHelper, macHelper);
    NetDeviceContainer gatewayDevices = lorawanHelper.InstallGateways(gateways, phyHelper, macHelper);

    // Set up the network topology (point-to-point between gateway and server)
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2pHelper.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NodeContainer networkNodes;
    networkNodes.Add(gateways.Get(0));

    NodeContainer serverNodes;
    serverNodes.Create(1);
    networkNodes.Add(serverNodes.Get(0));

    NetDeviceContainer p2pDevices = p2pHelper.Install(networkNodes);

    // Install the internet stack
    InternetStackHelper internetHelper;
    internetHelper.Install(networkNodes);
    internetHelper.Install(serverNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4Helper;
    ipv4Helper.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4Helper.Assign(p2pDevices);
    Ipv4InterfaceContainer serverInterfaces = ipv4Helper.Assign(serverNodes);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create the UDP application on the end device
    uint16_t port = 12345;
    UdpClientHelper clientHelper(serverInterfaces.GetAddress(0), port);
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(appPeriodSeconds)));
    clientHelper.SetAttribute("MaxPackets", UintegerValue(1000000));

    ApplicationContainer clientApps = clientHelper.Install(endDevices.Get(0));
    clientApps.Start(Seconds(1));
    clientApps.Stop(Seconds(simulationTime));

    // Create the UDP server application on the server
    UdpServerHelper serverHelper(port);
    ApplicationContainer serverApps = serverHelper.Install(serverNodes.Get(0));
    serverApps.Start(Seconds(0));
    serverApps.Stop(Seconds(simulationTime + 1));

    // Start the simulation
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}