#!/usr/bin/env python3
import argparse
from impl.supervisor import Supervisor


def menu():
    p = argparse.ArgumentParser()
    p.add_argument('--config', required=True)
    args = p.parse_args()
    sup = Supervisor(args.config)

    while True:
        print("\n=== DEMO MENU ===")
        print("1. Start workers")
        print("2. Stop all")
        print("3. Light Mode")
        print("4. Heavy Mode")
        print("5. Reload Config")
        print("6. Exit")
        choice = input("Choose option: ").strip()

        if choice == "1":
            sup.start_workers()
        elif choice == "2":
            sup.stop_all()
        elif choice == "3":
            sup.broadcast_light()
        elif choice == "4":
            sup.broadcast_heavy()
        elif choice == "5":
            sup.reload_config()
        elif choice == "6":
            sup.stop_all()
            print("Exiting. Log is closed")
            break
        else:
            print("Incorrect choice")

if __name__ == "__main__":
    menu()
