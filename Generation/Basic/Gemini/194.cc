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
    uint32_t numStaPerAp = 5;
    double simulationTime = 10.0; // seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("numStaPerAp", "Number of stations per AP", numStaPerAp);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    // 1. Channel configuration
    Ptr<YansWifiChannel> channel1 = CreateObject<YansWifiChannel>();
    Ptr<YansWifiChannel> channel2 = CreateObject<YansWifiChannel>();

    // Configure PHY for each channel
    HtWifiPhyHelper htPhy1;
    htPhy1.SetChannel(channel1);
    HtWifiPhyHelper htPhy2;
    htPhy2.SetChannel(channel2);

    // 2. Wi-Fi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ); // Using 802.11n 5GHz for higher throughput

    NqosWifiMacHelper mac;

    // 3. Nodes
    NodeContainer apNodes[2];
    NodeContainer staNodes[2];
    NetDeviceContainer apDevices[2];
    NetDeviceContainer staDevices[2];
    Ipv4InterfaceContainer apIpInterfaces[2];
    Ipv4InterfaceContainer staIpInterfaces[2];

    for (uint32_t i = 0; i < 2; ++i)
    {
        apNodes[i].Create(1);
        staNodes[i].Create(numStaPerAp);

        // Install Internet Stack on all nodes
        InternetStackHelper stack;
        stack.Install(apNodes[i]);
        stack.Install(staNodes[i]);

        // Configure AP MAC and install Wi-Fi devices
        Ssid ssid = (i == 0) ? Ssid("ns3-ap1") : Ssid("ns3-ap2");
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(MicroSeconds(102400)));
        if (i == 0)
        {
            apDevices[i] = wifi.Install(htPhy1, mac, apNodes[i]);
        }
        else
        {
            apDevices[i] = wifi.Install(htPhy2, mac, apNodes[i]);
        }

        // Configure STA MAC and install Wi-Fi devices
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false)); // Don't probe, directly associate
        if (i == 0)
        {
            staDevices[i] = wifi.Install(htPhy1, mac, staNodes[i]);
        }
        else
        {
            staDevices[i] = wifi.Install(htPhy2, mac, staNodes[i]);
        }

        // Assign IP addresses
        Ipv4AddressHelper ipv4;
        std::string networkAddress = (i == 0) ? "10.1.1.0" : "10.1.2.0";
        ipv4.SetBase(networkAddress.c_str(), "255.255.255.0");
        apIpInterfaces[i] = ipv4.Assign(apDevices[i]);
        staIpInterfaces[i] = ipv4.Assign(staDevices[i]);
    }

    // 4. Mobility
    MobilityHelper mobility;

    // AP1 location
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes[0]);
    apNodes[0].Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // AP2 location
    mobility.Install(apNodes[1]);
    apNodes[1].Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(50.0, 0.0, 0.0));

    // STAs for AP1 (around 0,0,0)
    Ptr<ListPositionAllocator> sta1PositionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numStaPerAp; ++i)
    {
        double x = (i % 2 == 0 ? 1 : -1) * (i / 2 + 1);
        double y = (i % 2 == 0 ? -1 : 1) * (i / 2 + 1);
        sta1PositionAlloc->Add(Vector(x, y, 0.0));
    }
    mobility.SetPositionAllocator(sta1PositionAlloc);
    mobility.Install(staNodes[0]);

    // STAs for AP2 (around 50,0,0)
    Ptr<ListPositionAllocator> sta2PositionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numStaPerAp; ++i)
    {
        double x = (i % 2 == 0 ? 1 : -1) * (i / 2 + 1);
        double y = (i % 2 == 0 ? -1 : 1) * (i / 2 + 1);
        sta2PositionAlloc->Add(Vector(50.0 + x, y, 0.0));
    }
    mobility.SetPositionAllocator(sta2PositionAlloc);
    mobility.Install(staNodes[1]);

    // 5. Applications (UDP Client/Server)
    ApplicationContainer serverApps[2];
    ApplicationContainer clientApps[2];

    for (uint32_t i = 0; i < 2; ++i)
    {
        uint16_t port = (i == 0) ? 9 : 10; // Different ports for different APs

        // Server on APs
        UdpServerHelper serverHelper(port);
        serverApps[i] = serverHelper.Install(apNodes[i]);
        serverApps[i].Start(Seconds(0.0));
        serverApps[i].Stop(Seconds(simulationTime + 1.0));

        // Clients on stations send to their respective APs
        Ipv4Address apIp = apIpInterfaces[i].GetAddress(0);
        UdpClientHelper clientHelper(apIp, port);
        clientHelper.SetAttribute("MaxPackets", UintegerValue(0));        // Continuous traffic
        clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(50))); // Send every 50ms
        clientHelper.SetAttribute("PacketSize", UintegerValue(1024));     // 1KB packets

        clientApps[i] = clientHelper.Install(staNodes[i]);
        clientApps[i].Start(Seconds(1.0)); // Start after a small delay
        clientApps[i].Stop(Seconds(simulationTime));
    }

    // 6. Tracing
    htPhy1.EnablePcap("wifi-ap1", apDevices[0].Get(0));
    htPhy1.EnablePcap("wifi-sta1-group", staDevices[0]); // Pcap for all STAs in group 1
    htPhy2.EnablePcap("wifi-ap2", apDevices[1].Get(0));
    htPhy2.EnablePcap("wifi-sta2-group", staDevices[1]); // Pcap for all STAs in group 2

    // Global ASCII trace (separate files for each channel)
    AsciiTraceHelper ascii;
    htPhy1.EnableAsciiAll(ascii.CreateFileStream("two-ap-wifi-simulation-phy1.tr"));
    htPhy2.EnableAsciiAll(ascii.CreateFileStream("two-ap-wifi-simulation-phy2.tr"));

    // 7. Simulation Run
    Simulator::Stop(Seconds(simulationTime + 1.0));
    Simulator::Run();

    // 8. Print total data sent by each station
    uint64_t totalDataSentOverall = 0;
    for (uint32_t i = 0; i < 2; ++i)
    {
        std::cout << "--- Group " << (i + 1) << " (AP" << (i + 1) << " with SSID " << ((i == 0) ? "ns3-ap1" : "ns3-ap2") << ") ---" << std::endl;
        for (uint32_t j = 0; j < numStaPerAp; ++j)
        {
            Ptr<UdpClient> client = DynamicCast<UdpClient>(clientApps[i].Get(j));
            uint64_t totalTxBytes = client->GetTotalTxBytes();
            std::cout << "  Station " << (j + 1) << " (Node ID " << staNodes[i].Get(j)->GetId() << ") sent "
                      << totalTxBytes << " bytes." << std::endl;
            totalDataSentOverall += totalTxBytes;
        }
    }
    std::cout << "Total data sent by all stations combined: " << totalDataSentOverall << " bytes." << std::endl;

    // 9. Cleanup
    Simulator::Destroy();

    return 0;
}