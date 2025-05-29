#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    uint32_t payloadSize = 128;
    double interval = 0.1; // 100 ms
    double simTime = 10.0;

    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    Ssid ssid = Ssid("ns3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces.Add(address.Assign(staDevice));
    interfaces.Add(address.Assign(apDevice));

    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(uint32_t(simTime / interval)));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApp = client.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    phy.EnablePcap("wifi-simulation", apDevice.Get(0));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}