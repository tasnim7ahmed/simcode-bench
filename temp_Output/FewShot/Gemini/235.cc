#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lorawan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/log.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    LogComponentEnable("LoraWanUdpExample", LOG_LEVEL_INFO);

    Config::SetDefault("ns3::LorawanPhy::Frequency", DoubleValue(915e6));
    Config::SetDefault("ns3::LorawanPhy::SF", UintegerValue(12));
    Config::SetDefault("ns3::LorawanPhy::Bandwidth", UintegerValue(125e3));
    Config::SetDefault("ns3::LorawanPhy::CodingRate", StringValue("4/5"));

    NodeContainer endDevices;
    endDevices.Create(1);

    NodeContainer gateways;
    gateways.Create(1);

    // Mobility
    MobilityHelper mobilityEd;
    mobilityEd.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                    "X", DoubleValue(50.0),
                                    "Y", DoubleValue(50.0),
                                    "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=20]"));
    mobilityEd.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEd.Install(endDevices);

    MobilityHelper mobilityGw;
    mobilityGw.SetPositionAllocator("ns3::ConstantPositionAllocator",
                                    "X", DoubleValue(100.0),
                                    "Y", DoubleValue(100.0),
                                    "Z", DoubleValue(0.0));
    mobilityGw.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGw.Install(gateways);

    // LoRaWAN
    LorawanHelper lorawanHelper;
    Ptr<LorawanChannel> channel = lorawanHelper.CreateChannel();
    LorawanPhyHelper phyHelper = lorawanHelper.GetPhy();
    phyHelper.SetChannel(channel);

    LorawanMacHelper macHelper;
    LorawanNetDeviceHelper lorawanNetDeviceHelper;

    NetDeviceContainer endDeviceDevices = lorawanNetDeviceHelper.Install(phyHelper, macHelper, endDevices);
    NetDeviceContainer gatewayDevices = lorawanNetDeviceHelper.Install(phyHelper, macHelper, gateways);

    lorawanHelper.SetEndDevices(endDeviceDevices);
    lorawanHelper.SetGateways(gatewayDevices);

    // Activate end devices
    macHelper.Activate(endDeviceDevices);

    // Internet
    InternetStackHelper internet;
    internet.Install(gateways);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(gatewayDevices);

    // UDP application
    uint16_t port = 5000;
    UdpServerHelper server(port);
    ApplicationContainer apps = server.Install(gateways.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(10));

    ApplicationContainer clientApps = client.Install(endDevices.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}